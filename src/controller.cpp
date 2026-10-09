#include "controller.h"

#include <QDir>
#include <QFutureWatcher>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QtConcurrent>

#include <HolonightFileBrowser/places/place_list.h>
#include <algorithm>
#include <fcntl.h>
#include <fnmatch.h>
#include <holonight/search/index.h>
#include <unistd.h>

namespace Portal {
namespace {
struct SearchEntry {
  QByteArray path;
  QString name;
  bool directory;
};
struct SearchResult {
  QSet<QByteArray> matches;
  QHash<QByteArray, QString> mime_names;
};
SearchResult searchFolder(const QList<SearchEntry>& records, const QString& query,
                          const std::shared_ptr<std::atomic_bool>& token, bool mimeNeeded) {
  HolonightSearch::Index index;
  QVector<HolonightSearch::Record> values;
  SearchResult result;
  for (const auto& entry : records) {
    if (token->load()) {
      return SearchResult{};
    }
    if (!query.isEmpty()) {
      values.append({.id = QString::fromLatin1(entry.path.toHex()),
                     .fields = {{.name = "name", .text = entry.name, .weight = 1}},
                     .boost = 0});
    }
    if (mimeNeeded && !entry.directory) {
      QByteArray bytes(4096, '\0');
      // POSIX open is variadic even when O_CREAT is absent.
      // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
      const int descriptor = ::open(entry.path.constData(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
      if (descriptor >= 0) {
        const auto size = ::read(descriptor, bytes.data(), static_cast<size_t>(bytes.size()));
        ::close(descriptor);
        bytes.resize(size > 0 ? size : 0);
      } else {
        bytes.clear();
      }
      result.mime_names.insert(entry.path, QMimeDatabase().mimeTypeForFileNameAndData(entry.name, bytes).name());
    }
  }
  if (query.isEmpty()) {
    return result;
  }
  index.replace("folder", std::move(values));
  HolonightSearch::Options options;
  options.limit = static_cast<int>(records.size());
  options.profile = HolonightSearch::Profile::Path;
  options.cancelled = token.get();
  for (const auto& match : index.search(query, options)) {
    result.matches.insert(QByteArray::fromHex(match.id.toLatin1()));
  }
  return result;
}
}  // namespace
bool EntryFilter::filterAcceptsRow(int row, const QModelIndex& parent) const {
  const auto index = sourceModel()->index(row, 0, parent);
  const bool directory = index.data(DirectoryRoles::IsDirRole).toBool();
  if (searching && !matches.contains(index.data(DirectoryRoles::NativePathRole).toByteArray())) {
    return false;
  }
  if (directory || filter.patterns.isEmpty()) {
    return true;
  }
  const auto name = index.data(DirectoryRoles::NameRole).toString();
  const auto native = index.data(DirectoryRoles::NativePathRole).toByteArray();
  const auto basename = native.mid(native.lastIndexOf('/') + 1);
  return std::ranges::any_of(filter.patterns, [&](const Pattern& pattern) {
    if (pattern.kind == 0) {
      return ::fnmatch(pattern.value.toUtf8().constData(), basename.constData(), 0) == 0;
    }
    if (pattern.kind != 1) {
      return false;
    }
    const auto mime = QMimeDatabase().mimeTypeForName(
        mime_names.value(native, QMimeDatabase().mimeTypeForFile(name, QMimeDatabase::MatchExtension).name()));
    return mime.inherits(pattern.value) ||
           (pattern.value.endsWith("/*") && mime.name().startsWith(pattern.value.chopped(1)));
  });
}
Controller::Controller(Options options, QObject* parent)
    : QObject(parent), options_(std::move(options)), reader_(new HolonightFileBrowser::DirectoryReader) {
  sorted_.setSourceModel(reader_);
  filter_.setSourceModel(&sorted_);
  connect(&sorted_, &DirectoryProxyModel::hiddenVisibleChanged, this, &Controller::hiddenVisibleChanged);
  const auto standard = PlaceList::standardPlaces(
      QDir::homePath(), QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/user-dirs.dirs");
  reader_->setPlaceIcons(PlaceList::iconMap(standard));
  connect(reader_, &HolonightFileBrowser::DirectoryReader::shutdownFinished, this, [this] { reader_retired_ = true; });
  for (const auto& item : standard) {
    places_.append(QVariantMap{{"name", item.name}, {"path", item.path}, {"iconName", item.icon_name}});
  }
  connect(reader_, &HolonightFileBrowser::DirectoryReader::changed, this, [this] {
    if (stopped_) {
      return;
    }
    if (!reader_->directoryError().isEmpty()) {
      explanation_ = reader_->directoryError();
    }
    refreshSearch();
    emit changed();
  });
  connect(reader_, &QAbstractItemModel::rowsInserted, this, [this] { refreshSearch(); });
  connect(reader_, &QAbstractItemModel::modelReset, this, [this] { refreshSearch(); });
  connect(&filter_, &QAbstractItemModel::modelReset, this, [this] { setCursor(cursor_); });
  connect(&filter_, &QAbstractItemModel::rowsRemoved, this, [this] { setCursor(cursor_); });
  if (!options_.filters.isEmpty()) {
    int index = static_cast<int>(options_.filters.indexOf(options_.current_filter));
    setFilterIndex(index < 0 ? 0 : index);
  } else if (!options_.current_filter.name.isEmpty()) {
    options_.filters.append(options_.current_filter);
    setFilterIndex(0);
  }
  for (auto& choice : options_.choices) {
    if (choice.values.isEmpty()) {
      if (choice.selected != "true" && choice.selected != "false") {
        choice.selected = "false";
      }
    } else if (std::ranges::none_of(choice.values, [&](const Pair& item) { return item.id == choice.selected; })) {
      choice.selected = choice.values.front().id;
    }
  }
  initializeLocation();
}
// Qt parents own watchers, including when the controller is cancelled before completion.
// NOLINTBEGIN(clang-analyzer-cplusplus.NewDeleteLeaks)
void Controller::initializeLocation() {
  auto* watcher = new QFutureWatcher<QPair<QByteArray, QByteArray>>(this);
  connect(watcher, &QFutureWatcherBase::finished, this,
          [this, watcher, navigation = navigation_generation_, filename = filename_generation_] {
            const auto result = watcher->result();
            watcher->deleteLater();
            if (stopped_) {
              return;
            }
            initializing_ = false;
            if (navigation != navigation_generation_) {
              emit changed();
              return;
            }
            if (filename == filename_generation_) {
              filename_ = result.second;
              emit filenameChanged();
            }
            if (result.first.isEmpty()) {
              explanation_ = "The suggested location is unavailable; showing Home.";
              navigateNative(QFile::encodeName(QDir::homePath()));
            } else {
              navigateNative(result.first);
            }
          });
  watcher->setFuture(QtConcurrent::run([options = options_] {
    auto folder = unterminate(options.folder);
    auto name = options.name.toUtf8();
    const auto file = unterminate(options.file);
    if (options.operation == Operation::Save && !file.isEmpty()) {
      if (!absolutePath(file)) {
        return qMakePair(QByteArray{}, name);
      }
      folder = parentPath(file);
      name = file.mid(file.lastIndexOf('/') + 1);
    }
    if (folder.isEmpty()) {
      folder = QFile::encodeName(QDir::homePath());
    }
    return qMakePair(directoryExists(folder) ? folder : QByteArray{}, name);
  }));
}
// NOLINTEND(clang-analyzer-cplusplus.NewDeleteLeaks)
Controller::~Controller() {
  stop();
  filter_.setSourceModel(nullptr);
  sorted_.setSourceModel(nullptr);
  // The provider must survive its possibly blocked worker. UI ownership ends now.
  if (reader_retired_) {
    reader_->deleteLater();
  } else {
    connect(reader_, &HolonightFileBrowser::DirectoryReader::shutdownFinished, reader_, &QObject::deleteLater);
  }
}
void Controller::stop() {
  if (stopped_) {
    return;
  }
  stopped_ = true;
  if (search_cancel_) {
    search_cancel_->store(true);
  }
  invalidateValidation();
  reader_->shutdown();
}
void Controller::invalidateValidation() {
  ++validation_generation_;
  if (validation_cancel_) {
    validation_cancel_->store(true);
  }
  validating_ = false;
  overwrite_pending_ = false;
}
QStringList Controller::selectedIds() const {
  QStringList ids;
  for (const auto& item : selected_) {
    ids.append(QString::fromLatin1(item.toHex()));
  }
  return ids;
}
QStringList Controller::filterNames() const {
  QStringList names;
  for (const auto& filter : options_.filters) {
    names.append(filter.name);
  }
  return names;
}
QVariantList Controller::choices() const {
  QVariantList values;
  for (const auto& choice : options_.choices) {
    QVariantList items;
    for (const auto& item : choice.values) {
      items.append(QVariantMap{{"id", item.id}, {"label", item.label}});
    }
    values.append(
        QVariantMap{{"id", choice.id}, {"label", choice.label}, {"selected", choice.selected}, {"items", items}});
  }
  return values;
}
void Controller::setChoice(int index, const QString& identifier) {
  if (index < 0 || index >= options_.choices.size()) {
    return;
  }
  auto& choice = options_.choices[index];
  if (choice.values.isEmpty()
          ? (identifier == "true" || identifier == "false")
          : std::ranges::any_of(choice.values, [&](const Pair& item) { return item.id == identifier; })) {
    choice.selected = identifier;
    emit choicesChanged();
  }
}
void Controller::setFilename(const QString& value) {
  // Preserve initial non-UTF-8 bytes when an unchanged display value is committed.
  if (value == filename()) {
    return;
  }
  invalidateValidation();
  ++filename_generation_;
  filename_ = value.toUtf8();
  emit filenameChanged();
  emit changed();
}
void Controller::setCursor(int value) {
  cursor_ = std::clamp(value, 0, std::max(0, filter_.rowCount() - 1));
  emit cursorChanged();
}
void Controller::setFilterIndex(int value) {
  if (value < 0 || value >= options_.filters.size()) {
    return;
  }
  filter_index_ = value;
  filter_.filter = options_.filters[value];
  filter_.refresh();
  refreshSearch();
  emit changed();
}
void Controller::setHiddenVisible(bool visible) { sorted_.setHiddenVisible(visible); }
void Controller::navigate(const QString& path) {
  navigateNative(path == location() ? folder_ : QFile::encodeName(path));
}
void Controller::navigateNative(const QByteArray& path) {
  if (stopped_ || !absolutePath(path)) {
    explanation_ = "Enter an absolute local path.";
    emit changed();
    return;
  }
  invalidateValidation();
  initializing_ = false;
  folder_ = path;
  ++navigation_generation_;
  selected_.clear();
  cursor_ = 0;
  query_.clear();
  ++search_generation_;
  if (search_cancel_) {
    search_cancel_->store(true);
  }
  filter_.searching = false;
  filter_.refresh();
  reader_->loadNative(folder_);
  emit locationChanged();
  emit queryChanged();
  emit selectionChanged();
  emit cursorChanged();
  emit changed();
}
void Controller::parentFolder() { navigateNative(parentPath(folder_)); }
void Controller::move(int delta) { select(std::clamp(cursor_ + delta, 0, std::max(0, filter_.rowCount() - 1))); }
QByteArray Controller::rowPath(int row) const {
  return filter_.index(row, 0).data(DirectoryRoles::NativePathRole).toByteArray();
}
bool Controller::selectable(int row) const {
  auto index = filter_.index(row, 0);
  return index.isValid() && !index.data(DirectoryRoles::IsParentRole).toBool() &&
         !index.data(DirectoryRoles::StatFailedRole).toBool() &&
         (options_.directory == index.data(DirectoryRoles::IsDirRole).toBool());
}
void Controller::select(int row) {
  if (row < 0 || row >= filter_.rowCount()) {
    return;
  }
  setCursor(row);
  if (!multiple() && !save()) {
    invalidateValidation();
    selected_.clear();
    if (selectable(row)) {
      selected_.append(rowPath(row));
    }
    emit selectionChanged();
    emit changed();
  }
}
void Controller::activate(int row) {
  if (row < 0 || row >= filter_.rowCount()) {
    return;
  }
  if (filter_.index(row, 0).data(DirectoryRoles::IsDirRole).toBool()) {
    navigateNative(rowPath(row));
  } else if (save()) {
    ++filename_generation_;
    filename_ = rowPath(row).mid(rowPath(row).lastIndexOf('/') + 1);
    emit filenameChanged();
    invalidateValidation();
    emit changed();
  } else if (!multiple() && selectable(row)) {
    select(row);
    accept();
  }
}
void Controller::toggle(int row) {
  if (!selectable(row)) {
    return;
  }
  invalidateValidation();
  const auto path = rowPath(row);
  if (selected_.contains(path)) {
    selected_.removeAll(path);
  } else {
    if (!multiple()) {
      selected_.clear();
    }
    selected_.append(path);
  }
  emit selectionChanged();
  emit changed();
}
void Controller::search(const QString& query) {
  query_ = query;
  emit queryChanged();
  refreshSearch();
}
void Controller::refreshSearch() {
  if (stopped_) {
    return;
  }
  ++search_generation_;
  if (search_cancel_) {
    search_cancel_->store(true);
  }
  filter_.searching = !query_.isEmpty();
  const bool mimeNeeded =
      std::ranges::any_of(filter_.filter.patterns, [](const Pattern& item) { return item.kind == 1; });
  if (query_.isEmpty() && !mimeNeeded) {
    filter_.refresh();
    return;
  }
  QList<SearchEntry> records;
  for (int row = 0; row < reader_->rowCount(); ++row) {
    auto index = reader_->index(row, 0);
    records.append({.path = index.data(DirectoryRoles::NativePathRole).toByteArray(),
                    .name = index.data(DirectoryRoles::NameRole).toString(),
                    .directory = index.data(DirectoryRoles::IsDirRole).toBool()});
  }
  const auto generation = search_generation_;
  const auto token = search_cancel_ = std::make_shared<std::atomic_bool>(false);
  auto* watcher = new QFutureWatcher<SearchResult>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation] {
    const auto result = watcher->result();
    watcher->deleteLater();
    if (stopped_ || generation != search_generation_) {
      return;
    }
    filter_.matches = result.matches;
    filter_.mime_names = result.mime_names;
    filter_.refresh();
    setCursor(cursor_);
  });
  watcher->setFuture(QtConcurrent::run([records = std::move(records), query = query_, token, mimeNeeded] {
    return searchFolder(records, query, token, mimeNeeded);
  }));
}
void Controller::accept() {
  if (!busy() && options_.operation == Operation::Open && !options_.directory && selected_.isEmpty() &&
      cursorIsDirectory()) {
    activate(cursor_);
    return;
  }
  startValidation(false);
}
void Controller::startValidation(bool overwriteConfirmed) {
  if (stopped_ || busy() || overwrite_pending_) {
    return;
  }
  QList<QByteArray> paths;
  if (save()) {
    if (!validName(filename_)) {
      explanation_ = "Enter a filename without path separators.";
      emit changed();
      return;
    }
    paths.append(joinPath(folder_, filename_));
  } else if (options_.operation == Operation::SaveMany) {
    paths.append(selected_.isEmpty() ? folder_ : selected_.front());
  } else if (!selected_.isEmpty()) {
    paths = selected_;
  } else if (options_.directory) {
    paths.append(folder_);
  } else if (!multiple() && selectable(cursor_)) {
    paths.append(rowPath(cursor_));
  }
  invalidateValidation();
  validating_ = true;
  emit changed();
  const auto generation = validation_generation_;
  const auto token = validation_cancel_ = std::make_shared<std::atomic_bool>(false);
  auto* watcher = new QFutureWatcher<Validation>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, generation, overwriteConfirmed] {
    const auto result = watcher->result();
    watcher->deleteLater();
    if (stopped_ || generation != validation_generation_) {
      return;
    }
    validating_ = false;
    if (!result.error.isEmpty()) {
      explanation_ = result.error;
    } else if (result.overwrite && !overwriteConfirmed) {
      overwrite_pending_ = true;
    } else {
      finish(result.paths);
    }
    emit changed();
  });
  watcher->setFuture(QtConcurrent::run([options = options_, paths, token] {
    return validate(options.operation, options.directory, paths, options.files, token);
  }));
}
void Controller::confirmOverwrite(bool confirm) {
  if (!overwrite_pending_ || stopped_) {
    return;
  }
  overwrite_pending_ = false;
  if (confirm) {
    startValidation(true);
  }
  emit changed();
}
void Controller::finish(const QList<QByteArray>& paths) {
  QStringList uris;
  for (const auto& path : paths) {
    uris.append(localUri(path));
  }
  QVariantMap results{{"uris", uris}};
  Pairs choices;
  for (const auto& choice : options_.choices) {
    choices.append({.id = choice.id, .label = choice.selected});
  }
  if (!choices.isEmpty()) {
    results.insert("choices", QVariant::fromValue(choices));
  }
  if (filter_index_ >= 0) {
    results.insert("current_filter", QVariant::fromValue(options_.filters[filter_index_]));
  }
  stop();
  emit completed(0, results);
}
void Controller::cancel() {
  if (!stopped_) {
    stop();
    emit completed(1, {});
  }
}
}  // namespace Portal
