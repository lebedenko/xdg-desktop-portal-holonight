#pragma once
#include "filesystem.h"

#include <QSet>
#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

#include <HolonightFileBrowser/directory_proxy_model.h>
#include <HolonightFileBrowser/directory_reader.h>
#include <HolonightFileBrowser/selection.h>

namespace Portal {
class EntryFilter : public QSortFilterProxyModel {
 public:
  static constexpr int NativeIdRole = Qt::UserRole + 100;
  [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override {
    if (role == NativeIdRole) {
      return QString::fromLatin1(
          QSortFilterProxyModel::data(index, DirectoryRoles::NativePathRole).toByteArray().toHex());
    }
    return QSortFilterProxyModel::data(index, role);
  }
  [[nodiscard]] QHash<int, QByteArray> roleNames() const override {
    auto roles = QSortFilterProxyModel::roleNames();
    roles.insert(NativeIdRole, "nativeId");
    return roles;
  }
  Filter filter;
  bool searching = false;
  QSet<QByteArray> matches;
  QHash<QByteArray, QString> mime_names;
  void refresh() {
    beginFilterChange();
    endFilterChange(Direction::Rows);
  }

 protected:
  [[nodiscard]] bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
};
class Controller : public QObject {
  Q_OBJECT
  QML_ELEMENT
  QML_UNCREATABLE("Created per portal request")
  Q_PROPERTY(QAbstractItemModel* entries READ entries CONSTANT)
  Q_PROPERTY(QString query READ query NOTIFY queryChanged)
  Q_PROPERTY(QString location READ location NOTIFY locationChanged)
  Q_PROPERTY(QString filename READ filename WRITE setFilename NOTIFY filenameChanged)
  Q_PROPERTY(QString explanation READ explanation NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool overwritePending READ overwritePending NOTIFY changed)
  Q_PROPERTY(bool multiple READ multiple CONSTANT)
  Q_PROPERTY(bool directoryMode READ directoryMode CONSTANT)
  Q_PROPERTY(bool save READ save CONSTANT)
  Q_PROPERTY(bool hiddenVisible READ hiddenVisible WRITE setHiddenVisible NOTIFY hiddenVisibleChanged)
  Q_PROPERTY(QString title READ title CONSTANT)
  Q_PROPERTY(QString acceptLabel READ acceptLabel CONSTANT)
  Q_PROPERTY(int cursor READ cursor WRITE setCursor NOTIFY cursorChanged)
  Q_PROPERTY(QStringList selectedIds READ selectedIds NOTIFY selectionChanged)
  Q_PROPERTY(QVariantList places READ places CONSTANT)
  Q_PROPERTY(QStringList filterNames READ filterNames CONSTANT)
  Q_PROPERTY(int filterIndex READ filterIndex WRITE setFilterIndex NOTIFY changed)
  Q_PROPERTY(QVariantList choices READ choices NOTIFY choicesChanged)
 public:
  explicit Controller(Options options, QObject* parent = nullptr);
  ~Controller() override;
  Controller(const Controller&) = delete;
  Controller& operator=(const Controller&) = delete;
  Controller(Controller&&) = delete;
  Controller& operator=(Controller&&) = delete;
  QAbstractItemModel* entries() { return &filter_; }
  [[nodiscard]] QString query() const { return query_; }
  [[nodiscard]] QString location() const { return QString::fromUtf8(folder_); }
  [[nodiscard]] QString filename() const { return QString::fromUtf8(filename_); }
  [[nodiscard]] QString explanation() const { return explanation_; }
  [[nodiscard]] bool busy() const { return validating_ || initializing_ || reader_->scanning(); }
  [[nodiscard]] bool overwritePending() const { return overwrite_pending_; }
  [[nodiscard]] bool multiple() const { return options_.multiple; }
  [[nodiscard]] bool directoryMode() const { return options_.directory; }
  [[nodiscard]] bool save() const { return options_.operation == Operation::Save; }
  [[nodiscard]] bool hiddenVisible() const { return sorted_.hiddenVisible(); }
  [[nodiscard]] QString title() const { return options_.title; }
  [[nodiscard]] QString acceptLabel() const { return options_.accept_label; }
  [[nodiscard]] int cursor() const { return cursor_; }
  [[nodiscard]] QStringList selectedIds() const;
  [[nodiscard]] QVariantList places() const { return places_; }
  [[nodiscard]] QStringList filterNames() const;
  [[nodiscard]] int filterIndex() const { return filter_index_; }
  [[nodiscard]] QVariantList choices() const;
  void setFilename(const QString& value);
  void setCursor(int value);
  void setFilterIndex(int value);
  void setHiddenVisible(bool visible);
  Q_INVOKABLE [[nodiscard]] bool cursorIsDirectory() const {
    return filter_.index(cursor_, 0).data(DirectoryRoles::IsDirRole).toBool();
  }
  Q_INVOKABLE void setChoice(int index, const QString& identifier);
  Q_INVOKABLE void navigate(const QString& path);
  void navigateNative(const QByteArray& path);
  Q_INVOKABLE void parentFolder();
  Q_INVOKABLE void move(int delta);
  Q_INVOKABLE void select(int row);
  Q_INVOKABLE void activate(int row);
  Q_INVOKABLE void toggle(int row);
  Q_INVOKABLE void search(const QString& query);
  Q_INVOKABLE void accept();
  Q_INVOKABLE void confirmOverwrite(bool confirm);
  Q_INVOKABLE void cancel();
  void stop();
 signals:
  void changed();
  void queryChanged();
  void locationChanged();
  void filenameChanged();
  void cursorChanged();
  void selectionChanged();
  void choicesChanged();
  void hiddenVisibleChanged();
  void completed(uint response, const QVariantMap& results);

 private:
  [[nodiscard]] QByteArray rowPath(int row) const;
  [[nodiscard]] bool selectable(int row) const;
  void initializeLocation();
  void refreshSearch();
  void startValidation(bool overwriteConfirmed);
  void finish(const QList<QByteArray>& paths);
  void invalidateValidation();
  Options options_;
  HolonightFileBrowser::DirectoryReader* reader_;
  DirectoryProxyModel sorted_;
  EntryFilter filter_;
  QByteArray folder_;
  QByteArray filename_;
  HolonightFileBrowser::Selection selection_;
  QVariantList places_;
  QString explanation_;
  QString query_;
  int cursor_ = 0;
  int filter_index_ = -1;
  quint64 search_generation_ = 0;
  quint64 validation_generation_ = 0;
  quint64 navigation_generation_ = 0;
  quint64 filename_generation_ = 0;
  std::shared_ptr<std::atomic_bool> search_cancel_;
  std::shared_ptr<std::atomic_bool> validation_cancel_;
  bool initializing_ = true;
  bool validating_ = false;
  bool overwrite_pending_ = false;
  bool stopped_ = false;
  bool reader_retired_ = false;
};
}  // namespace Portal
