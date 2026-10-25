#include "backend.h"
#include "presenter.h"
#include "wayland_parent.h"

#include <QDBusConnectionInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QFile>
#include <QPointer>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QXmlStreamReader>

#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace Portal;
namespace {
Options folderOptions(const QByteArray& folder) {
  Options options;
  options.folder = folder;
  options.title = "Test chooser";
  options.accept_label = "Open";
  return options;
}
void writeFile(const QByteArray& path) {
  // Native-path fixtures require POSIX open, whose ABI is variadic.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
  const int descriptor = ::open(path.constData(), O_CREAT | O_WRONLY | O_TRUNC, 0600);
  if (descriptor < 0) {
    qFatal("Cannot create the native-path test fixture.");
  }
  const auto ignored = ::write(descriptor, "unchanged", 9);
  (void)ignored;
  ::close(descriptor);
}
int findRow(Controller& controller, const QString& name) {
  for (int row = 0; row < controller.entries()->rowCount(); ++row) {
    if (controller.entries()->index(row, 0).data(DirectoryRoles::NameRole).toString() == name) {
      return row;
    }
  }
  return -1;
}
QQuickItem* findNamedItem(QQuickItem* parent, const QString& name) {
  if (parent->objectName() == name) {
    return parent;
  }
  for (auto* child : parent->childItems()) {
    if (auto* item = findNamedItem(child, name)) {
      return item;
    }
  }
  return nullptr;
}
QQuickItem* findEntry(QQuickItem* parent, const QString& title) {
  if (parent->property("title").toString() == title) {
    return parent;
  }
  for (auto* child : parent->childItems()) {
    if (auto* entry = findEntry(child, title)) {
      return entry;
    }
  }
  return nullptr;
}
class TestSurfaceRole : public QObject {
  Q_OBJECT
 public:
  void createRole() { emit surfaceRoleCreated(); }

 signals:
  void surfaceRoleCreated();
};
class DelayedParent : public WaylandParent {
 public:
  explicit DelayedParent(QObject* owner) : WaylandParent(owner) {}
  void finish(bool valid = true) { complete(valid); }
  void observeRole(TestSurfaceRole* role) { watchRole(role); }
  QStringList events;

 protected:
  void prepareNative() override {}
  void applyParent() override { events.append("parent"); }
};
class TestPresenter : public Presenter {
 public:
  QList<Controller*> shown;
  bool fail = false;
  bool exported_before_show = false;
  QDBusConnection connection = QDBusConnection::sessionBus();
  QString expected_handle;
  bool show(Controller* controller, const Options& /*options*/) override {
    exported_before_show = connection.objectRegisteredAt(expected_handle) != nullptr;
    shown.append(controller);
    return !fail;
  }
  void close(Controller* controller) override { shown.removeAll(controller); }
};
QDBusConnection clientBus() { return QDBusConnection::connectToBus(QDBusConnection::SessionBus, "test-client"); }
QDBusPendingCall callChooser(const QDBusConnection& bus, const QString& method, const QString& handle,
                             const QVariantMap& options = {}) {
  auto message =
      QDBusMessage::createMethodCall(BusName, DesktopPath, "org.freedesktop.impl.portal.FileChooser", method);
  message.setArguments({QVariant::fromValue(QDBusObjectPath(handle)), "test.app", "", "Test", options});
  return bus.asyncCall(message);
}
QDBusPendingCall closeRequest(const QDBusConnection& bus, const QString& handle) {
  return bus.asyncCall(QDBusMessage::createMethodCall(BusName, handle, "org.freedesktop.impl.portal.Request", "Close"));
}
}  // namespace
// QtTest discovers these non-static QObject slots by name.
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class PortalTests : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase() { registerWireTypes(); }
  void delayedParentPreparation() {
    QTemporaryDir directory;
    Controller controller(folderOptions(directory.path().toUtf8()));
    QPointer<DelayedParent> adapter;
    QPointer<QWindow> window;
    QmlPresenter presenter([&](QWindow* created, const QString&) {
      window = created;
      adapter = new DelayedParent(created);
      return adapter.data();
    });
    QVERIFY(presenter.show(&controller, folderOptions(directory.path().toUtf8())));
    QVERIFY(window);
    QVERIFY(!window->isVisible());
    adapter->finish();
    QVERIFY(window->isVisible());
    QSignalSpy visible(window, &QWindow::visibleChanged);
    adapter->finish();
    QCOMPARE(visible.size(), 0);
    presenter.close(&controller);
    QVERIFY(!adapter);
  }
  void parentAppliedDuringRoleCreation() {
    TestSurfaceRole role;
    DelayedParent adapter(nullptr);
    adapter.observeRole(&role);
    adapter.prepare();
    role.createRole();
    QVERIFY(adapter.events.isEmpty());
    connect(&adapter, &WaylandParent::prepared, &role, [&] {
      adapter.events.append("show");
      role.createRole();
      adapter.events.append("initial-commit");
    });
    adapter.finish();
    QCOMPARE(adapter.events, QStringList({"show", "parent", "initial-commit"}));
  }
  void invalidImportedParent() {
    TestSurfaceRole role;
    DelayedParent adapter(nullptr);
    adapter.observeRole(&role);
    QSignalSpy prepared(&adapter, &WaylandParent::prepared);
    adapter.prepare();
    adapter.finish(false);
    QCOMPARE(prepared.size(), 1);
    role.createRole();
    QVERIFY(adapter.events.isEmpty());
  }
  void parentPreparationTimeout() {
    QTemporaryDir directory;
    Controller controller(folderOptions(directory.path().toUtf8()));
    QPointer<QWindow> window;
    QPointer<DelayedParent> adapter;
    TestSurfaceRole role;
    QmlPresenter presenter([&](QWindow* created, const QString&) {
      window = created;
      adapter = new DelayedParent(created);
      adapter->observeRole(&role);
      return adapter.data();
    });
    QVERIFY(presenter.show(&controller, folderOptions(directory.path().toUtf8())));
    QVERIFY(!window->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(window->isVisible(), 1500);
    adapter->finish();
    role.createRole();
    QVERIFY(adapter->events.isEmpty());
    presenter.close(&controller);
  }
  void cancelParentPreparation() {
    QTemporaryDir directory;
    Controller controller(folderOptions(directory.path().toUtf8()));
    QPointer<DelayedParent> adapter;
    QPointer<QWindow> window;
    QmlPresenter presenter([&](QWindow* created, const QString&) {
      window = created;
      adapter = new DelayedParent(created);
      return adapter.data();
    });
    QVERIFY(presenter.show(&controller, folderOptions(directory.path().toUtf8())));
    QVERIFY(!window->isVisible());
    presenter.close(&controller);
    QVERIFY(!adapter);
    QVERIFY(!window->isVisible());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY(!window);
  }
  // QtTest requires the _data suffix.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void unavailableParent_data() {
    QTest::addColumn<QString>("handle");
    QTest::newRow("missing") << QString{};
    QTest::newRow("unsupported") << QString("x11:123");
    QTest::newRow("empty-wayland") << QString("wayland:");
    QTest::newRow("embedded-null") << QString::fromLatin1("wayland:a\0b", 11);
    QTest::newRow("no-wayland-platform") << QString("wayland:unknown");
  }
  void unavailableParent() {
    QFETCH(QString, handle);
    QWindow window;
    window.create();
    WaylandParent adapter(&window, handle, &window);
    QSignalSpy prepared(&adapter, &WaylandParent::prepared);
    adapter.prepare();
    QCOMPARE(prepared.size(), 1);
  }
  void activationRuntime() {
    if (!qEnvironmentVariableIsSet("PORTAL_ACTIVATION_TEST")) {
      QSKIP("Only run with the staged activation fixture.");
    }
    const auto bus = QDBusConnection::sessionBus();
    QVERIFY(!bus.interface()->isServiceRegistered(BusName).value());
    const auto readSettings = [&] {
      auto message =
          QDBusMessage::createMethodCall(BusName, DesktopPath, "org.freedesktop.impl.portal.Settings", "Read");
      message.setArguments({"org.freedesktop.appearance", "color-scheme"});
      return bus.asyncCall(message);
    };
    uint settingsPid = 0;
    if (qEnvironmentVariableIsSet("PORTAL_SETTINGS_FIRST")) {
      auto settings = readSettings();
      QTRY_VERIFY(settings.isFinished());
      QCOMPARE(settings.reply().signature(), "v");
      settingsPid = bus.interface()->servicePid(BusName).value();
    }
    auto call = callChooser(bus, "OpenFile", "/test/activation");
    QTRY_VERIFY(bus.interface()->isServiceRegistered(BusName).value());
    // Wait for export through ordinary asynchronous introspection; no nested D-Bus calls.
    QDBusMessage reply;
    QTRY_VERIFY(([&] {
      auto pending = bus.asyncCall(QDBusMessage::createMethodCall(BusName, "/test/activation",
                                                                  "org.freedesktop.DBus.Introspectable", "Introspect"));
      QDBusPendingCallWatcher watcher(pending);
      QSignalSpy spy(&watcher, &QDBusPendingCallWatcher::finished);
      if (!pending.isFinished()) spy.wait(1000);
      reply = pending.reply();
      return reply.type() == QDBusMessage::ReplyMessage;
    })());
    QVERIFY(reply.arguments().front().toString().contains("org.freedesktop.impl.portal.Request"));
    auto close = closeRequest(bus, "/test/activation");
    QTRY_VERIFY(close.isFinished());
    QTRY_VERIFY(call.isFinished());
    QDBusPendingReply<uint, QVariantMap> result(call);
    QCOMPARE(result.argumentAt<0>(), 1U);
    const auto pid = bus.interface()->servicePid(BusName);
    QVERIFY(pid.isValid());
    if (settingsPid != 0) {
      QCOMPARE(pid.value(), settingsPid);
    }
    auto settings = readSettings();
    QTRY_VERIFY(settings.isFinished());
    QCOMPARE(settings.reply().signature(), "v");
    QCOMPARE(bus.interface()->servicePid(BusName).value(), pid.value());
    auto shutdownCall = callChooser(bus, "SaveFile", "/test/shutdown");
    QTRY_VERIFY(([&] {
      auto pending = bus.asyncCall(QDBusMessage::createMethodCall(BusName, "/test/shutdown",
                                                                  "org.freedesktop.DBus.Introspectable", "Introspect"));
      QDBusPendingCallWatcher watcher(pending);
      QSignalSpy spy(&watcher, &QDBusPendingCallWatcher::finished);
      if (!pending.isFinished()) {
        spy.wait(1000);
      }
      return pending.reply().type() == QDBusMessage::ReplyMessage;
    })());
    QVERIFY(::kill(static_cast<pid_t>(pid.value()), SIGTERM) == 0);
    QTRY_VERIFY(shutdownCall.isFinished());
    QDBusPendingReply<uint, QVariantMap> stopped(shutdownCall);
    QCOMPARE(stopped.argumentAt<0>(), 2U);
    QTRY_VERIFY(!bus.interface()->isServiceRegistered(BusName).value());
  }
  void wireTypes() {
    QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Filters>())), "a(sa(us))");
    QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Choices>())), "a(ssa(ss)s)");
    QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Pairs>())), "a(ss)");
    QCOMPARE(QByteArray(QDBusMetaType::typeToSignature(QMetaType::fromType<Names>())), "aay");
  }
  void uriPreservesBytes() {
    QCOMPARE(localUri(QByteArray("/tmp/a #?%") + char(0xff)), "file:///tmp/a%20%23%3F%25%FF");
    QCOMPARE(localUri(QString("/tmp/文").toUtf8()), "file:///tmp/%E6%96%87");
    QVERIFY(localUri("relative").isEmpty());
    QVERIFY(localUri(QByteArray("/a\0b", 4)).isEmpty());
  }
  void saveCollisionsLeaveFilesystemUnchanged() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    writeFile(joinPath(folder, "report.txt"));
    writeFile(joinPath(folder, "report (1).txt"));
    const auto odd = QByteArray("bad") + static_cast<char>(0xff) + ".txt";
    const auto before = QDir(temp.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    auto token = std::make_shared<std::atomic_bool>(false);
    auto result = validate(Operation::SaveMany, true, {folder}, {"report.txt", "report.txt", odd, odd}, token);
    QVERIFY2(result.error.isEmpty(), qPrintable(result.error));
    QCOMPARE(result.paths,
             (QList<QByteArray>{joinPath(folder, "report (2).txt"), joinPath(folder, "report (3).txt"),
                                joinPath(folder, odd), joinPath(folder, QByteArray("bad") + char(0xff) + " (1).txt")}));
    QCOMPARE(QDir(temp.path()).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot), before);
    QFile existing(temp.filePath("report.txt"));
    QVERIFY(existing.open(QIODevice::ReadOnly));
    QCOMPARE(existing.readAll(), "unchanged");
    result = validate(Operation::Save, false, {joinPath(folder, "report.txt")}, {}, token);
    QVERIFY(result.overwrite);
    for (const auto& name : Names{"", ".", "..", "a/b", QByteArray("a\0b", 3)}) {
      QVERIFY(!validate(Operation::SaveMany, true, {folder}, {name}, token).error.isEmpty());
    }
    QVERIFY(!validate(Operation::Save, false, {folder + "/missing/file"}, {}, token).error.isEmpty());
  }
  void childOnlyListingsAndParentNavigation() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    QVERIFY(QDir(temp.path()).mkdir("empty"));
    for (int mode = 0; mode < 3; ++mode) {
      auto options = folderOptions(joinPath(folder, "empty"));
      options.operation = mode == 1 ? Operation::Save : Operation::Open;
      options.directory = mode == 2;
      QmlPresenter presenter;
      Controller controller(options);
      QVERIFY(presenter.show(&controller, options));
      QTRY_VERIFY(!controller.busy());
      QCOMPARE(controller.entries()->rowCount(), 0);
      QCOMPARE(findRow(controller, ".."), -1);
      QQuickWindow* window = nullptr;
      for (auto* candidate : QGuiApplication::topLevelWindows()) {
        if (candidate->isVisible()) {
          window = qobject_cast<QQuickWindow*>(candidate);
        }
      }
      QVERIFY(window);
      auto* listing = window->findChild<QQuickItem*>("listing");
      auto* parentButton = window->findChild<QQuickItem*>("parentFolder");
      QVERIFY(listing && parentButton);
      QVERIFY(QMetaObject::invokeMethod(parentButton, "clicked"));
      QTRY_VERIFY(!controller.busy());
      QVERIFY(findRow(controller, "empty") >= 0);
      QCOMPARE(findRow(controller, ".."), -1);
      controller.activate(findRow(controller, "empty"));
      QTRY_VERIFY(!controller.busy());
      QCOMPARE(controller.entries()->rowCount(), 0);
      listing->forceActiveFocus();
      QTest::keyClick(window, Qt::Key_Left);
      QTRY_VERIFY(!controller.busy());
      QVERIFY(findRow(controller, "empty") >= 0);
      QCOMPARE(findRow(controller, ".."), -1);
      presenter.close(&controller);
      QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
  }
  void selectionAndFiltering() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    writeFile(joinPath(folder, "file2.txt"));
    writeFile(joinPath(folder, "file10.png"));
    writeFile(joinPath(folder, ".hidden"));
    QVERIFY(QDir(temp.path()).mkdir("sub"));
    auto options = folderOptions(folder);
    options.multiple = true;
    options.filters = {{.name = "Text", .patterns = {{.kind = 0, .value = "*.txt"}}},
                       {.name = "Images", .patterns = {{.kind = 1, .value = "image/png"}}}};
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    QVERIFY(findRow(controller, "sub") >= 0);
    QVERIFY(findRow(controller, "file2.txt") >= 0);
    QCOMPARE(findRow(controller, "file10.png"), -1);
    controller.toggle(findRow(controller, "file2.txt"));
    const auto selection = controller.selectedIds();
    controller.move(1);
    QCOMPARE(controller.selectedIds(), selection);
    controller.setFilterIndex(1);
    QCOMPARE(controller.selectedIds(), selection);
    QVERIFY(findRow(controller, "file10.png") >= 0);
    controller.search("file10");
    QTRY_VERIFY(findRow(controller, "sub") < 0);
    QCOMPARE(controller.selectedIds(), selection);
    controller.search("nonsense");
    controller.search("");
    QTRY_VERIFY(findRow(controller, "sub") >= 0);
    controller.accept();
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[0].toUInt(), 0U);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(),
             QStringList{localUri(joinPath(folder, "file2.txt"))});
  }
  void folderSelectionAndNavigation() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    QVERIFY(QDir(temp.path()).mkdir("child"));
    auto options = folderOptions(folder);
    options.directory = true;
    options.multiple = true;
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    controller.toggle(findRow(controller, "child"));
    QVERIFY(!controller.selectedIds().isEmpty());
    controller.activate(findRow(controller, "child"));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.selectedIds().isEmpty());
    controller.accept();
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(), QStringList{localUri(joinPath(folder, "child"))});
  }
  void singleFileActivationReplacesSelection() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    writeFile(joinPath(folder, "first"));
    writeFile(joinPath(folder, "second"));
    Controller controller(folderOptions(folder));
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    controller.toggle(findRow(controller, "first"));
    controller.activate(findRow(controller, "second"));
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[0].toUInt(), 0U);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(), QStringList{localUri(joinPath(folder, "second"))});
  }
  void singleFileKeyboardMovementUpdatesSelection() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    writeFile(joinPath(folder, "first"));
    writeFile(joinPath(folder, "second"));
    Controller controller(folderOptions(folder));
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    controller.select(findRow(controller, "first"));
    controller.move(1);
    controller.accept();
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(), QStringList{localUri(joinPath(folder, "second"))});
  }
  // QtTest requires the _data suffix.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void singleFolderClickSelectsDestination_data() {
    QTest::addColumn<bool>("saveMany");
    QTest::addColumn<bool>("previousSelection");
    QTest::newRow("open-folder") << false << false;
    QTest::newRow("save-files") << true << false;
    QTest::newRow("open-folder-reselect") << false << true;
    QTest::newRow("save-files-reselect") << true << true;
  }
  void singleFolderClickSelectsDestination() {
    QFETCH(bool, saveMany);
    QFETCH(bool, previousSelection);
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    QVERIFY(QDir(temp.path()).mkdir("child"));
    QVERIFY(QDir(temp.path()).mkdir("previous"));
    auto options = folderOptions(folder);
    options.directory = true;
    if (saveMany) {
      options.operation = Operation::SaveMany;
      options.files = {"out.txt"};
    }
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QmlPresenter presenter;
    QVERIFY(presenter.show(&controller, options));
    QTRY_VERIFY(!controller.busy());
    if (previousSelection) {
      controller.toggle(findRow(controller, "previous"));
    }
    QQuickItem* entry = nullptr;
    const auto locateEntry = [&] {
      for (auto* window : QGuiApplication::topLevelWindows()) {
        auto* quickWindow = qobject_cast<QQuickWindow*>(window);
        if (!quickWindow || !window->isVisible()) {
          continue;
        }
        entry = findEntry(quickWindow->contentItem(), "child");
        if (entry) {
          return true;
        }
      }
      return false;
    };
    QTRY_VERIFY(locateEntry());
    // Invoke the delegate's action directly without desktop pointer/focus automation.
    QVERIFY(QMetaObject::invokeMethod(entry, "clicked"));
    QVERIFY(QMetaObject::invokeMethod(entry, "clicked"));
    controller.accept();
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[0].toUInt(), 0U);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(),
             QStringList{localUri(joinPath(folder, saveMany ? "child/out.txt" : "child"))});
    presenter.close(&controller);
  }
  void hiddenFilesCanBeRevealedAndOpened() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    writeFile(joinPath(folder, ".hidden"));
    Controller controller(folderOptions(folder));
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(findRow(controller, ".hidden"), -1);
    QVERIFY(controller.setProperty("hiddenVisible", true));
    QTRY_VERIFY(findRow(controller, ".hidden") >= 0);
    QVERIFY(controller.setProperty("hiddenVisible", false));
    QCOMPARE(findRow(controller, ".hidden"), -1);
    QVERIFY(controller.setProperty("hiddenVisible", true));
    controller.activate(findRow(controller, ".hidden"));
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(), QStringList{localUri(joinPath(folder, ".hidden"))});
  }
  void initialNativeSaveAndOverwrite() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    const auto name = QByteArray("odd") + static_cast<char>(0xff);
    writeFile(joinPath(folder, name));
    auto options = folderOptions("/missing");
    options.operation = Operation::Save;
    options.file = joinPath(folder, name) + '\0';
    options.name = "ignored";
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.location(), temp.path());
    controller.setFilename(controller.filename());
    controller.accept();
    QTRY_VERIFY(controller.overwritePending());
    QCOMPARE(done.size(), 0);
    controller.confirmOverwrite(false);
    QCOMPARE(done.size(), 0);
    controller.accept();
    QTRY_VERIFY(controller.overwritePending());
    controller.confirmOverwrite(true);
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(), QStringList{localUri(joinPath(folder, name))});
    controller.cancel();
    QCOMPARE(done.size(), 1);
    QVERIFY(::unlink(joinPath(folder, name).constData()) == 0);
  }
  void invalidInitialAndRemovedEntry() {
    auto options = folderOptions("/definitely/missing");
    Controller fallback(options);
    QTRY_VERIFY(!fallback.busy());
    QCOMPARE(fallback.location(), QDir::homePath());
    QVERIFY(!fallback.explanation().isEmpty());
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    writeFile(joinPath(folder, "gone"));
    Controller controller(folderOptions(folder));
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    controller.setCursor(findRow(controller, "gone"));
    QVERIFY(QFile::remove(temp.filePath("gone")));
    controller.accept();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(done.size(), 0);
    QVERIFY(!controller.explanation().isEmpty());
  }
  void initialWorkCannotReplaceUserNavigation() {
    QTemporaryDir initial;
    QTemporaryDir chosen;
    auto options = folderOptions(QFile::encodeName(initial.path()));
    options.operation = Operation::Save;
    options.name = "initial";
    Controller controller(options);
    controller.navigate(chosen.path());
    controller.setFilename("chosen");
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.location(), chosen.path());
    QCOMPARE(controller.filename(), "chosen");
  }
  void navigationHistoryPreservesNativePathsAndBranches() {
    QTemporaryDir temp;
    const auto base = QFile::encodeName(temp.path());
    const auto native = joinPath(base, QByteArray("native-\xff"));
    QVERIFY(::mkdir(native.constData(), 0700) == 0);
    QVERIFY(QDir(temp.path()).mkdir("other"));
    writeFile(joinPath(native, "a"));
    auto options = folderOptions(base);
    options.directory = true;
    Controller controller(options);
    QTRY_VERIFY(!controller.busy());
    QVERIFY(!controller.canGoBack() && !controller.canGoForward());
    controller.navigateNative(native);
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.canGoBack());
    controller.goBack();
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.location(), temp.path());
    QVERIFY(!controller.canGoBack() && controller.canGoForward());
    controller.goForward();
    QTRY_VERIFY(!controller.busy());
    QVERIFY(findRow(controller, "a") >= 0);
    QSignalSpy completed(&controller, &Controller::completed);
    controller.accept();
    QTRY_COMPARE(completed.size(), 1);
    QCOMPARE(completed.first().at(1).toMap().value("uris").toStringList(), QStringList{localUri(native)});

    Controller branch(folderOptions(base));
    QTRY_VERIFY(!branch.busy());
    branch.navigateNative(native);
    QTRY_VERIFY(!branch.busy());
    branch.goBack();
    QTRY_VERIFY(!branch.busy());
    branch.navigateNative(base);
    QVERIFY(branch.canGoForward());
    branch.navigateNative(joinPath(base, "other"));
    QTRY_VERIFY(!branch.busy());
    QVERIFY(!branch.canGoForward());
    branch.goForward();
    QCOMPARE(branch.location(), temp.filePath("other"));
    branch.navigate("relative");
    branch.goBack();
    QTRY_VERIFY(!branch.busy());
    QCOMPARE(branch.location(), temp.path());
    QVERIFY(!branch.canGoBack());
  }

  void nonUtf8FolderLocationRoundTrip() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path()) + "/folder" + static_cast<char>(0xff);
    QVERIFY(::mkdir(folder.constData(), 0700) == 0);
    auto options = folderOptions(folder + '\0');
    options.operation = Operation::Save;
    options.name = "new #%.txt";
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    controller.navigate(controller.location());
    QTRY_VERIFY(!controller.busy());
    controller.accept();
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[1].toMap().value("uris").toStringList(),
             QStringList{localUri(joinPath(folder, "new #%.txt"))});
    QVERIFY(::rmdir(folder.constData()) == 0);
  }
  void mimeContentAndStaleFolderWork() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    QFile png(temp.filePath("picture"));
    QVERIFY(png.open(QIODevice::WriteOnly));
    png.write(QByteArray::fromHex("89504e470d0a1a0a0000000d49484452"));
    png.close();
    QVERIFY(QDir(temp.path()).mkdir("child"));
    auto options = folderOptions(folder);
    options.filters = {{.name = "PNG", .patterns = {{.kind = 1, .value = "image/png"}}}};
    Controller controller(options);
    QTRY_VERIFY(!controller.busy());
    QTRY_VERIFY(findRow(controller, "picture") >= 0);
    QVERIFY(findRow(controller, "child") >= 0);
    controller.search("picture");
    controller.navigate(temp.filePath("child"));
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(findRow(controller, "picture"), -1);
    QCOMPARE(controller.query(), "");
  }
  void staleNavigationAndValidation() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    auto options = folderOptions(folder);
    options.operation = Operation::Save;
    options.name = "new";
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    controller.accept();
    controller.setFilename("other");
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(done.size(), 0);
    controller.accept();
    controller.cancel();
    QTRY_COMPARE(done.size(), 1);
    QCOMPARE(done.front()[0].toUInt(), 1U);
    QVERIFY(!QFile::exists(temp.filePath("new")));
    QVERIFY(!QFile::exists(temp.filePath("other")));
  }
  // QtTest requires the _data suffix.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void allOpenSelectionModes_data() {
    QTest::addColumn<bool>("directory");
    QTest::addColumn<bool>("multiple");
    QTest::newRow("single-file") << false << false;
    QTest::newRow("multiple-files") << false << true;
    QTest::newRow("single-folder") << true << false;
    QTest::newRow("multiple-folders") << true << true;
  }
  void allOpenSelectionModes() {
    QFETCH(bool, directory);
    QFETCH(bool, multiple);
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    for (const auto& name : {"first", "second"}) {
      if (directory) {
        QVERIFY(QDir(temp.path()).mkdir(name));
      } else {
        writeFile(joinPath(folder, name));
      }
    }
    auto options = folderOptions(folder);
    options.directory = directory;
    options.multiple = multiple;
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    controller.toggle(findRow(controller, "first"));
    controller.toggle(findRow(controller, "second"));
    controller.accept();
    QTRY_COMPARE(done.size(), 1);
    const auto uris = done.front()[1].toMap().value("uris").toStringList();
    QCOMPARE(uris.size(), multiple ? 2 : 1);
    if (multiple) {
      QCOMPARE(uris.front(), localUri(joinPath(folder, "first")));
    }
    QCOMPARE(uris.back(), localUri(joinPath(folder, "second")));
  }
  void nativeSelectionDistinguishesEqualDisplayNames() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    const auto first = joinPath(folder, QByteArray("entry") + static_cast<char>(0xfe));
    const auto second = joinPath(folder, QByteArray("entry") + static_cast<char>(0xff));
    writeFile(first);
    writeFile(second);
    auto options = folderOptions(folder);
    options.multiple = true;
    Controller controller(options);
    QSignalSpy done(&controller, &Controller::completed);
    QTRY_VERIFY(!controller.busy());
    for (int row = 0; row < controller.entries()->rowCount(); ++row) {
      const auto path = controller.entries()->index(row, 0).data(DirectoryRoles::NativePathRole).toByteArray();
      if (path == first || path == second) {
        controller.toggle(row);
      }
    }
    QCOMPARE(controller.selectedIds().size(), 2);
    controller.accept();
    QTRY_COMPARE(done.size(), 1);
    const auto uris = done.front()[1].toMap().value("uris").toStringList();
    QVERIFY(uris.contains(localUri(first)));
    QVERIFY(uris.contains(localUri(second)));
    QVERIFY(::unlink(first.constData()) == 0);
    QVERIFY(::unlink(second.constData()) == 0);
  }
  void saveFilesWireAndDestinationSelection() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    QVERIFY(QDir(temp.path()).mkdir("child"));
    TestPresenter presenter;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    const auto names = Names{QByteArray("same.txt\0", 9), QByteArray("same.txt\0", 9)};
    auto call = callChooser(clientBus(), "SaveFiles", "/test/many",
                            {{"current_folder", folder + '\0'}, {"files", QVariant::fromValue(names)}});
    QTRY_COMPARE(presenter.shown.size(), 1);
    auto* controller = presenter.shown.front();
    QTRY_VERIFY(!controller->busy());
    controller->toggle(findRow(*controller, "child"));
    controller->accept();
    QTRY_VERIFY(call.isFinished());
    QDBusPendingReply<uint, QVariantMap> reply(call);
    QCOMPARE(reply.argumentAt<0>(), 0U);
    QCOMPARE(reply.argumentAt<1>().value("uris").toStringList(),
             (QStringList{localUri(folder + "/child/same.txt"), localUri(folder + "/child/same (1).txt")}));
    QVERIFY(QDir(temp.filePath("child")).entryList(QDir::Files).isEmpty());
  }
  void cancelDuringValidationAndReuseHandle() {
    QTemporaryDir temp;
    TestPresenter presenter;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    auto call = callChooser(clientBus(), "SaveFile", "/test/race",
                            {{"current_folder", QFile::encodeName(temp.path())}, {"current_name", "new"}});
    QTRY_COMPARE(presenter.shown.size(), 1);
    auto* controller = presenter.shown.front();
    QTRY_VERIFY(!controller->busy());
    controller->accept();
    controller->cancel();
    QTRY_VERIFY(call.isFinished());
    QDBusPendingReply<uint, QVariantMap> reply(call);
    QCOMPARE(reply.argumentAt<0>(), 1U);
    QCOMPARE(backend.activeRequests(), 0);
    QVERIFY(!QFile::exists(temp.filePath("new")));
    auto reused = callChooser(clientBus(), "OpenFile", "/test/race");
    QTRY_COMPARE(presenter.shown.size(), 1);
    presenter.shown.front()->cancel();
    QTRY_VERIFY(reused.isFinished());
    QCOMPARE(backend.activeRequests(), 0);
  }
  void simultaneousDuplicateAndClose() {
    TestPresenter presenter;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    auto bus = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "broker");
    presenter.expected_handle = "/test/one";
    auto first = callChooser(bus, "OpenFile", "/test/one");
    QTRY_COMPARE(presenter.shown.size(), 1);
    QVERIFY(presenter.exported_before_show);
    auto second = callChooser(bus, "SaveFile", "/test/two");
    QTRY_COMPARE(backend.activeRequests(), 2);
    auto duplicate = callChooser(bus, "SaveFiles", "/test/one");
    QTRY_VERIFY(duplicate.isFinished());
    QDBusPendingReply<uint, QVariantMap> dup(duplicate);
    QCOMPARE(dup.argumentAt<0>(), 2U);
    auto close = closeRequest(bus, "/test/one");
    QTRY_VERIFY(close.isFinished());
    QTRY_VERIFY(first.isFinished());
    QDBusPendingReply<uint, QVariantMap> result(first);
    QCOMPARE(result.argumentAt<0>(), 1U);
    QCOMPARE(backend.activeRequests(), 1);
    QCOMPARE(presenter.shown.size(), 1);
    backend.shutdown();
    QTRY_VERIFY(second.isFinished());
    QDBusPendingReply<uint, QVariantMap> stopped(second);
    QCOMPARE(stopped.argumentAt<0>(), 2U);
    QCOMPARE(backend.activeRequests(), 0);
    QDBusConnection::disconnectFromBus("broker");
  }
  void brokerLossAndUnauthorizedClose() {
    TestPresenter presenter;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    auto broker = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "lost-broker");
    auto pending = callChooser(broker, "OpenFile", "/test/lost");
    QTRY_COMPARE(backend.activeRequests(), 1);
    auto denied = closeRequest(QDBusConnection::sessionBus(), "/test/lost");
    QTRY_VERIFY(denied.isFinished());
    QVERIFY(denied.reply().type() == QDBusMessage::ErrorMessage);
    QCOMPARE(backend.activeRequests(), 1);
    broker = QDBusConnection("invalid");
    QDBusConnection::disconnectFromBus("lost-broker");
    QTRY_COMPARE(backend.activeRequests(), 0);
    QVERIFY(presenter.shown.isEmpty());
  }
  void wireRoundTripAndIntrospection() {
    QTemporaryDir temp;
    TestPresenter presenter;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    auto broker = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "wire-broker");
    const Filters filters{
        {.name = "Named text filter", .patterns = {{.kind = 0, .value = "*.txt"}, {.kind = 1, .value = "text/plain"}}}};
    const Choices choices{{.id = "encoding",
                           .label = "Encoding",
                           .values = {{.id = "utf8", .label = "UTF-8"}, {.id = "latin", .label = "Latin"}},
                           .selected = "latin"},
                          {.id = "boolean", .label = "Flag", .values = {}, .selected = "true"}};
    auto call = callChooser(broker, "SaveFile", "/test/wire",
                            {{"filters", QVariant::fromValue(filters)},
                             {"choices", QVariant::fromValue(choices)},
                             {"current_folder", QFile::encodeName(temp.path()) + '\0'},
                             {"current_name", "out.txt"}});
    QTRY_COMPARE(presenter.shown.size(), 1);
    auto* controller = presenter.shown.front();
    QTRY_VERIFY(!controller->busy());
    controller->accept();
    QTRY_VERIFY(call.isFinished());
    QDBusPendingReply<uint, QVariantMap> reply(call);
    QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
    QCOMPARE(reply.argumentAt<0>(), 0U);
    const auto results = reply.argumentAt<1>();
    QCOMPARE(decode<Filter>(results.value("current_filter")), filters.front());
    QCOMPARE(decode<Pairs>(results.value("choices")), (Pairs{{"encoding", "latin"}, {"boolean", "true"}}));
    auto introspect = broker.asyncCall(
        QDBusMessage::createMethodCall(BusName, DesktopPath, "org.freedesktop.DBus.Introspectable", "Introspect"));
    QTRY_VERIFY(introspect.isFinished());
    const auto xml = introspect.reply().arguments().front().toString();
    QVERIFY(xml.contains("name=\"SaveFiles\""));
    QVERIFY(xml.contains("type=\"a{sv}\""));
    QMap<QString, QPair<QString, QString>> signatures;
    QXmlStreamReader reader(xml);
    QString method;
    while (!reader.atEnd()) {
      reader.readNext();
      if (reader.isStartElement() && reader.name() == "method") {
        const auto name = reader.attributes().value("name").toString();
        method = QStringList{"OpenFile", "SaveFile", "SaveFiles"}.contains(name) ? name : QString{};
        if (!method.isEmpty()) {
          signatures.insert(method, {});
        }
      } else if (reader.isStartElement() && reader.name() == "arg" && !method.isEmpty()) {
        auto& signature = signatures[method];
        const auto type = reader.attributes().value("type").toString();
        if (reader.attributes().value("direction") == "in") {
          signature.first += type;
        } else {
          signature.second += type;
        }
      } else if (reader.isEndElement() && reader.name() == "method") {
        method.clear();
      }
    }
    QVERIFY(!reader.hasError());
    QCOMPARE(signatures.size(), 3);
    for (const auto& signature : signatures) {
      QCOMPARE(signature.first, "osssa{sv}");
      QCOMPARE(signature.second, "ua{sv}");
    }
    QCOMPARE(call.reply().signature(), "ua{sv}");
    QDBusConnection::disconnectFromBus("wire-broker");
  }
  void presentationFailure() {
    TestPresenter presenter;
    presenter.fail = true;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    auto call = callChooser(clientBus(), "OpenFile", "/test/failure");
    QTRY_VERIFY(call.isFinished());
    QDBusPendingReply<uint, QVariantMap> reply(call);
    QCOMPARE(reply.argumentAt<0>(), 2U);
    QCOMPARE(backend.activeRequests(), 0);
  }
  void previewFollowsCursorAndCancellation() {
    QTemporaryDir dir;
    QImage image(16, 8, QImage::Format_RGB32);
    image.fill(Qt::red);
    QVERIFY(image.save(dir.filePath("a.png")));
    writeFile(QFile::encodeName(dir.filePath("b.txt")));
    auto options = folderOptions(QFile::encodeName(dir.path()));
    options.multiple = true;
    Controller controller(options);
    QTRY_VERIFY(!controller.busy());
    const auto imageRow = findRow(controller, "a.png");
    const auto textRow = findRow(controller, "b.txt");
    QVERIFY(imageRow >= 0 && textRow >= 0);
    controller.toggle(textRow);
    const auto selected = controller.selectedIds();
    controller.select(imageRow);
    QTRY_VERIFY(controller.preview()->hasImage());
    QCOMPARE(controller.preview()->name(), QString("a.png"));
    QCOMPARE(controller.selectedIds(), selected);
    controller.select(textRow);
    QCOMPARE(controller.preview()->name(), QString("b.txt"));
    QTRY_VERIFY(!controller.preview()->busy());
    QVERIFY(!controller.preview()->hasImage());
    controller.cancel();
    QVERIFY(!controller.preview()->hasEntry());
  }

  void previewRetiresAfterRequestDestruction() {
    QTemporaryDir dir;
    QVERIFY(QDir(dir.path()).mkdir("initial"));
    QImage image(32, 16, QImage::Format_RGB32);
    image.fill(Qt::blue);
    QVERIFY(image.save(dir.filePath("pending.png")));
    QPointer<PreviewService> service;
    {
      Controller controller(folderOptions(QFile::encodeName(dir.path())));
      service = controller.preview();
      QTRY_VERIFY(!controller.busy());
      controller.setCursor(findRow(controller, "pending.png"));
      QVERIFY(service->busy());
    }
    // Cancellation retires the service after its worker, outside controller destruction.
    QVERIFY(!service.isNull());
    QVERIFY(!service->hasEntry());
    QTRY_VERIFY(service.isNull());
  }

  void compactPreviewCanBeRevealedWithoutChangingSelection() {
    QTemporaryDir dir;
    writeFile(QFile::encodeName(dir.filePath("image.png")));
    auto options = folderOptions(QFile::encodeName(dir.path()));
    QmlPresenter presenter;
    Controller controller(options);
    QVERIFY(presenter.show(&controller, options));
    QTRY_VERIFY(!controller.busy());
    QQuickWindow* window = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows()) {
      if (candidate->isVisible()) {
        window = qobject_cast<QQuickWindow*>(candidate);
      }
    }
    if (window == nullptr) {
      QFAIL("Missing chooser window");
    }
    window->resize(1000, 600);
    auto* preview = window->findChild<QQuickItem*>("entryPreview");
    auto* toggle = window->findChild<QQuickItem*>("previewToggle");
    QVERIFY(preview && toggle);
    QTRY_VERIFY(preview->isVisible());
    window->resize(800, 400);
    QTRY_VERIFY(!preview->isVisible() && toggle->isVisible());
    controller.select(findRow(controller, "image.png"));
    const auto cursor = controller.cursor();
    const auto selection = controller.selectedIds();
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_VERIFY(preview->isVisible());
    QCOMPARE(controller.selectedIds(), selection);
    auto* drawer = window->findChild<QObject*>("previewDrawer");
    QVERIFY(drawer);
    QTRY_VERIFY(drawer->property("opened").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!preview->isVisible());
    QVERIFY(window->isVisible());
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_VERIFY(preview->isVisible());
    window->resize(1000, 400);
    QTRY_VERIFY(window->property("wide").toBool());
    QTRY_VERIFY(preview->isVisible());
    window->resize(800, 400);
    QTRY_VERIFY(!window->property("wide").toBool());
    QTRY_VERIFY(!preview->isVisible());
    QCOMPARE(controller.cursor(), cursor);
    QCOMPARE(controller.selectedIds(), selection);
    presenter.close(&controller);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }

  void additionalOptionsRemainScrollableAndPersist() {
    QTemporaryDir dir;
    auto options = folderOptions(QFile::encodeName(dir.path()));
    options.multiple = true;
    for (int index = 0; index < 30; ++index) {
      options.choices.append({.id = QString::number(index),
                              .label = QString("A long caller option label ").repeated(8),
                              .values = {},
                              .selected = "false"});
    }
    QmlPresenter presenter;
    Controller controller(options);
    QVERIFY(presenter.show(&controller, options));
    QTRY_VERIFY(!controller.busy());
    QQuickWindow* window = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows()) {
      if (candidate->isVisible()) {
        window = qobject_cast<QQuickWindow*>(candidate);
      }
    }
    QVERIFY(window);
    window->resize(600, 400);
    auto* toggle = window->findChild<QQuickItem*>("optionsToggle");
    auto* popup = window->findChild<QObject*>("optionsPopup");
    auto* scroll = window->findChild<QQuickItem*>("optionsScroll");
    QVERIFY(toggle && popup && scroll);
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_VERIFY(popup->property("visible").toBool());
    QTRY_VERIFY(scroll->property("contentHeight").toReal() > scroll->height());
    auto* popupContent = qvariant_cast<QQuickItem*>(popup->property("contentItem"));
    QVERIFY(popupContent);
    auto* choice = findNamedItem(popupContent, "choice-0");
    QVERIFY(choice);
    choice->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(choice, "clicked"));
    QCOMPARE(controller.choices().first().toMap().value("selected").toString(), QString("true"));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!popup->property("visible").toBool());
    QVERIFY(window->isVisible());
    QVERIFY(QMetaObject::invokeMethod(toggle, "clicked"));
    QTRY_VERIFY(popup->property("visible").toBool());
    choice = findNamedItem(popupContent, "choice-0");
    QVERIFY(choice);
    QVERIFY(choice->property("checked").toBool());
    auto* flickable = qvariant_cast<QQuickItem*>(scroll->property("contentItem"));
    QVERIFY(flickable);
    flickable->setProperty("contentY", scroll->property("contentHeight").toReal() - scroll->height());
    auto* last = findNamedItem(popupContent, "choice-29");
    QVERIFY(last);
    QTRY_VERIFY(last->mapToScene(QPointF()).y() < window->height());
    last->setProperty("checked", true);
    QVERIFY(QMetaObject::invokeMethod(last, "clicked"));
    QCOMPARE(controller.choices().last().toMap().value("selected").toString(), QString("true"));
    auto* accept = window->findChild<QQuickItem*>("accept");
    QVERIFY(accept);
    QVERIFY(accept->mapToScene(QPointF(accept->width(), accept->height())).x() <= window->width());
    presenter.close(&controller);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }

  void placesHighlightOnlyTheCurrentLocation() {
    QTemporaryDir temp;
    QTemporaryDir config;
    QVERIFY(QDir(temp.path()).mkdir("documents"));
    QFile userDirs(config.filePath("user-dirs.dirs"));
    QVERIFY(userDirs.open(QIODevice::WriteOnly));
    QVERIFY(userDirs.write("XDG_DOCUMENTS_DIR=\"" + QFile::encodeName(temp.filePath("documents")) + "\"\n") > 0);
    userDirs.close();
    const bool hadConfig = qEnvironmentVariableIsSet("XDG_CONFIG_HOME");
    const auto previousConfig = qgetenv("XDG_CONFIG_HOME");
    const auto restoreConfig = qScopeGuard([&] {
      if (hadConfig) {
        qputenv("XDG_CONFIG_HOME", previousConfig);
      } else {
        qunsetenv("XDG_CONFIG_HOME");
      }
    });
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(config.path()));
    const auto options = folderOptions(QFile::encodeName(temp.path()));
    Controller controller(options);
    QmlPresenter presenter;
    QVERIFY(presenter.show(&controller, options));
    QTRY_VERIFY(!controller.busy());
    QQuickWindow* window = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows()) {
      if (candidate->isVisible()) {
        window = qobject_cast<QQuickWindow*>(candidate);
      }
    }
    QVERIFY(window);
    auto* places = window->findChild<QQuickItem*>("places");
    QVERIFY(places);
    const auto entries = controller.places();
    QVERIFY(entries.size() >= 2);
    const auto home = entries.at(0).toMap();
    const auto next = entries.at(1).toMap();
    QQuickItem* homeDelegate = nullptr;
    QQuickItem* nextDelegate = nullptr;
    QTRY_VERIFY((homeDelegate = findEntry(places, home.value("name").toString())));
    QTRY_VERIFY((nextDelegate = findEntry(places, next.value("name").toString())));
    QVERIFY(!homeDelegate->property("selected").toBool());
    QVERIFY(!nextDelegate->property("selected").toBool());
    QVERIFY(QMetaObject::invokeMethod(homeDelegate, "clicked"));
    QTRY_COMPARE(controller.location(), home.value("path").toString());
    QTRY_VERIFY(homeDelegate->property("selected").toBool());
    QVERIFY(!nextDelegate->property("selected").toBool());
    QVERIFY(QMetaObject::invokeMethod(nextDelegate, "clicked"));
    QTRY_COMPARE(controller.location(), next.value("path").toString());
    QTRY_VERIFY(!homeDelegate->property("selected").toBool());
    QTRY_VERIFY(nextDelegate->property("selected").toBool());
    controller.goBack();
    QTRY_COMPARE(controller.location(), home.value("path").toString());
    QTRY_VERIFY(homeDelegate->property("selected").toBool());
    QVERIFY(!nextDelegate->property("selected").toBool());
    controller.navigate(temp.path());
    QTRY_VERIFY(!homeDelegate->property("selected").toBool());
    QVERIFY(!nextDelegate->property("selected").toBool());
    presenter.close(&controller);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }

  void qmlFolderOpenAndKeyboardRecovery() {
    QTest::failOnWarning(QRegularExpression(".*image://icon/undefined.*"));
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    QVERIFY(QDir(temp.path()).mkdir("child"));
    writeFile(joinPath(folder, "child/a"));
    writeFile(joinPath(folder, "child/b"));
    Controller controller(folderOptions(folder));
    QmlPresenter presenter;
    QVERIFY(presenter.show(&controller, folderOptions(folder)));
    QTRY_VERIFY(!controller.busy());
    QQuickWindow* window = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows()) {
      if (candidate->isVisible()) {
        window = qobject_cast<QQuickWindow*>(candidate);
      }
    }
    if (window == nullptr) {
      QFAIL("Chooser window was not created.");
      return;
    }
    auto* content = window->contentItem();
    QQuickItem* entry = nullptr;
    QTRY_VERIFY((entry = findEntry(content, "child")));
    auto* listing = window->findChild<QQuickItem*>("listing");
    QVERIFY(listing);
    entry->forceActiveFocus();
    QVERIFY(QMetaObject::invokeMethod(entry, "clicked"));
    QTRY_VERIFY(listing->hasActiveFocus());
    auto* accept = window->findChild<QQuickItem*>("accept");
    QVERIFY(accept);
    accept->forceActiveFocus();
    QVERIFY(QMetaObject::invokeMethod(accept, "clicked"));
    QTRY_COMPARE(controller.location(), temp.filePath("child"));
    QTRY_VERIFY(!controller.busy());
    auto* back = window->findChild<QQuickItem*>("historyBack");
    auto* forward = window->findChild<QQuickItem*>("historyForward");
    QVERIFY(back && forward);
    QTRY_VERIFY(back->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(back, "clicked"));
    QTRY_COMPARE(controller.location(), temp.path());
    QTRY_VERIFY(!controller.busy());
    QTRY_VERIFY(forward->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(forward, "clicked"));
    QTRY_COMPARE(controller.location(), temp.filePath("child"));
    QTRY_VERIFY(!controller.busy());
    QVERIFY(controller.explanation().isEmpty());
    QTRY_VERIFY((entry = findEntry(content, "a")));
    entry->forceActiveFocus();
    QVERIFY(QMetaObject::invokeMethod(entry, "clicked"));
    QTRY_VERIFY(listing->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_J);
    QCOMPARE(controller.cursor(), findRow(controller, "b"));
    presenter.close(&controller);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
  void emptyFileSelectionExplainsRequiredType() {
    QTemporaryDir temp;
    auto options = folderOptions(QFile::encodeName(temp.path()));
    options.multiple = true;
    writeFile(joinPath(options.folder, "a"));
    Controller controller(options);
    QTRY_VERIFY(!controller.busy());
    controller.setCursor(findRow(controller, "a"));
    controller.accept();
    QTRY_COMPARE(controller.explanation(), QString("Select a file."));
  }
  // QtTest requires the _data suffix.
  // NOLINTNEXTLINE(readability-identifier-naming)
  void qmlBoundedDialog_data() {
    QTest::addColumn<bool>("save");
    QTest::addColumn<bool>("directory");
    QTest::addColumn<bool>("multiple");
    QTest::addColumn<QString>("purposeText");
    QTest::newRow("OpenFile") << false << false << false << QString("Open File");
    QTest::newRow("OpenFiles") << false << false << true << QString("Open Files");
    QTest::newRow("OpenFolder") << false << true << false << QString("Select Folder");
    QTest::newRow("OpenFolders") << false << true << true << QString("Select Folders");
    QTest::newRow("SaveFile") << true << false << false << QString("Save File");
    QTest::newRow("SaveFiles") << false << true << false << QString("Save Files");
  }
  void qmlBoundedDialog() {
    QFETCH(bool, save);
    QFETCH(bool, directory);
    QFETCH(bool, multiple);
    QFETCH(QString, purposeText);
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    for (int index = 0; index < 80; ++index) {
      writeFile(joinPath(folder, QByteArray::number(index)));
    }
    auto options = folderOptions(folder);
    options.directory = directory;
    options.multiple = multiple;
    options.filters = {{.name = "All files", .patterns = {{.kind = 0, .value = "*"}}}};
    if (purposeText == "Save Files") {
      options.operation = Operation::SaveMany;
    }
    if (save) {
      options.operation = Operation::Save;
      options.name = "new";
    }
    QmlPresenter presenter;
    Controller controller(options);
    QVERIFY(presenter.show(&controller, options));
    QTRY_VERIFY(!controller.busy());
    QQuickWindow* window = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows()) {
      if (candidate->isVisible()) {
        window = qobject_cast<QQuickWindow*>(candidate);
      }
    }
    QVERIFY(window);
    QCOMPARE(window->type(), Qt::Dialog);
    const int limit = qMax(1, qMin(600, window->screen()->availableGeometry().height() - 48));
    QCOMPARE(window->width(), qMax(600, qMin(1160, window->screen()->availableGeometry().width() - 48)));
    QCOMPARE(window->height(), limit);
    QCOMPARE(window->maximumHeight(), limit);
    QCOMPARE(window->minimumHeight(), qMin(400, limit));
    auto* accept = window->findChild<QQuickItem*>("accept");
    auto* cancel = window->findChild<QQuickItem*>("cancel");
    auto* listing = window->findChild<QQuickItem*>("listing");
    QVERIFY(accept);
    QVERIFY(cancel);
    QVERIFY(listing);
    auto* purpose = window->findChild<QQuickItem*>("purpose");
    QVERIFY(purpose);
    QCOMPARE(purpose->property("title").toString(), purposeText);
    auto* places = window->findChild<QQuickItem*>("places");
    QVERIFY(places);
    auto* sidebar = window->findChild<QQuickItem*>("placesPanel");
    auto* previewDock = window->findChild<QQuickItem*>("previewDock");
    auto* toolbar = window->findChild<QQuickItem*>("toolbarSurface");
    QVERIFY(sidebar && previewDock && toolbar);
    QCOMPARE(sidebar->width(), 220);
    auto* back = window->findChild<QQuickItem*>("historyBack");
    auto* forward = window->findChild<QQuickItem*>("historyForward");
    auto* parentButton = window->findChild<QQuickItem*>("parentFolder");
    QVERIFY(back && forward && parentButton);
    QVERIFY(!back->isEnabled() && !forward->isEnabled());
    QVERIFY(!window->findChild<QQuickItem*>("showHidden"));
    for (const auto* name : {"purposeSurface", "toolbarSurface", "placesPanel", "previewDock", "footerSurface"}) {
      QCOMPARE(window->findChild<QQuickItem*>(name)->property("color"), sidebar->property("color"));
    }
    QCOMPARE(window->minimumWidth(), 600);
    for (int width : {1160, 800, 600}) {
      for (int height : {limit, window->minimumHeight()}) {
        window->resize(width, height);
        QTRY_COMPARE(toolbar->mapToScene(QPointF(toolbar->width(), 0)).x(), window->width());
        QCOMPARE(toolbar->mapToScene(QPointF()).x(), sidebar->width());
        if (width >= 900) {
          QTRY_COMPARE(previewDock->width(), sidebar->width());
          QVERIFY(previewDock->mapToScene(QPointF()).y() >= toolbar->mapToScene(QPointF(0, toolbar->height())).y());
        }
        for (auto* button : {accept, cancel}) {
          QTRY_VERIFY(button->isVisible() && button->height() > 0);
          QTRY_VERIFY(button->mapToScene(QPointF(0, button->height())).y() <= window->height());
        }
        QQuickItem* filter = nullptr;
        QTRY_VERIFY((filter = window->findChild<QQuickItem*>("filter")) && filter->isVisible());
        auto* form = window->findChild<QQuickItem*>("browserForm");
        QVERIFY(form);
        if (save || width < 900) {
          QTRY_VERIFY(form->isVisible());
          QVERIFY(filter->mapToScene(QPointF()).y() >= listing->mapToScene(QPointF(0, listing->height())).y());
        } else {
          QTRY_VERIFY(!form->isVisible());
          QVERIFY(filter->mapToScene(QPointF()).y() >= accept->mapToScene(QPointF()).y() - 8);
        }
        QTRY_VERIFY(listing->height() > 0);
        QTRY_VERIFY(listing->property("contentHeight").toReal() > listing->height());
        QVERIFY(QMetaObject::invokeMethod(listing, "positionViewAtEnd"));
        QTRY_VERIFY(listing->property("contentY").toReal() > 0);
      }
    }
    presenter.close(&controller);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
  void qmlKeyboardAndFilenameFocus() {
    QTemporaryDir temp;
    const auto folder = QFile::encodeName(temp.path());
    writeFile(joinPath(folder, "a"));
    writeFile(joinPath(folder, "b"));
    writeFile(joinPath(folder, ".hidden"));
    QmlPresenter presenter;
    auto options = folderOptions(folder);
    options.multiple = true;
    Controller controller(options);
    QVERIFY(presenter.show(&controller, options));
    QTRY_VERIFY(!controller.busy());
    QQuickWindow* window = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows()) {
      if (candidate->isVisible()) {
        window = qobject_cast<QQuickWindow*>(candidate);
      }
    }
    QVERIFY(window);
    QVERIFY(!window->findChild<QQuickItem*>("showHidden"));
    QCOMPARE(findRow(controller, ".hidden"), -1);
    window->findChild<QQuickItem*>("listing")->forceActiveFocus();
    QTest::keyClick(window, Qt::Key_H, Qt::ControlModifier);
    QTRY_VERIFY(findRow(controller, ".hidden") >= 0);
    QTest::keyClick(window, Qt::Key_H, Qt::ControlModifier);
    QTRY_COMPARE(findRow(controller, ".hidden"), -1);
    QVERIFY(!controller.hiddenVisible());
    auto* listing = window->findChild<QQuickItem*>("listing");
    QVERIFY(listing);
    controller.setCursor(findRow(controller, "a"));
    auto* accept = window->findChild<QQuickItem*>("accept");
    QVERIFY(accept);
    accept->forceActiveFocus();
    QVERIFY(QMetaObject::invokeMethod(accept, "clicked"));
    QTRY_COMPARE(controller.explanation(), QString("Select a file."));
    QQuickItem* clickedEntry = nullptr;
    QTRY_VERIFY((clickedEntry = findEntry(window->contentItem(), "a")));
    clickedEntry->forceActiveFocus();
    QVERIFY(QMetaObject::invokeMethod(clickedEntry, "clicked"));
    QTRY_VERIFY(listing->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_J);
    QCOMPARE(controller.cursor(), findRow(controller, "b"));
    controller.setCursor(findRow(controller, "a"));
    QTest::keyClick(window, Qt::Key_Space);
    const auto selection = controller.selectedIds();
    QVERIFY(!selection.isEmpty());
    controller.select(findRow(controller, "b"));
    QCOMPARE(controller.selectedIds(), selection);
    controller.setCursor(findRow(controller, "a"));
    QTest::keyClick(window, Qt::Key_J);
    QCOMPARE(controller.selectedIds(), selection);
    QCOMPARE(controller.cursor(), findRow(controller, "b"));
    QTest::keyClick(window, Qt::Key_L, Qt::ControlModifier);
    QTRY_VERIFY(window->findChild<QQuickItem*>("location")->hasActiveFocus());
    presenter.close(&controller);
    options.operation = Operation::Save;
    options.name = "new";
    Controller save(options);
    QVERIFY(presenter.show(&save, options));
    QTRY_VERIFY(!save.busy());
    window = nullptr;
    for (auto* candidate : QGuiApplication::topLevelWindows()) {
      if (candidate->isVisible()) {
        window = qobject_cast<QQuickWindow*>(candidate);
      }
    }
    QVERIFY(window);
    QTRY_VERIFY(window->findChild<QQuickItem*>("filename")->hasActiveFocus());
    presenter.close(&save);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
  }
};
// NOLINTEND(readability-convert-member-functions-to-static)
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QQuickStyle::setStyle("Holonight");
  PortalTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "test_portal.moc"
