#include "backend.h"
#include "settings.h"

#include <QDBusConnectionInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFile>
#include <QSaveFile>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace Portal;
namespace {
constexpr auto AppearanceNs = "org.freedesktop.appearance";
Holonight::ResolvedAppearance makeAppearance(QString scheme, QString accent, Holonight::ColorMode mode) {
  Holonight::ResolvedAppearance result;
  result.scheme = std::move(scheme);
  result.accent = std::move(accent);
  result.color_mode = mode;
  return result;
}
class NoPresenter : public Presenter {
 public:
  bool show(Controller* /*controller*/, const Options& /*options*/) override { return false; }
  void close(Controller* /*controller*/) override {}
};
QDBusMessage settingsCall(const QString& method, const QVariantList& arguments) {
  auto message = QDBusMessage::createMethodCall(BusName, DesktopPath, "org.freedesktop.impl.portal.Settings", method);
  message.setArguments(arguments);
  auto pending = QDBusConnection::sessionBus().asyncCall(message);
  QDBusPendingCallWatcher watcher(pending);
  QSignalSpy finished(&watcher, &QDBusPendingCallWatcher::finished);
  if (!pending.isFinished()) {
    finished.wait(2000);
  }
  return pending.reply();
}
bool replace(const QString& path, const QByteArray& contents) {
  QSaveFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size() && file.commit();
}
}  // namespace
class SettingsSignals : public QObject {
  Q_OBJECT
 public slots:
  void changed(const QString& space, const QString& key, const QDBusVariant& value) {
    emit received(space, key, value);
  }
 signals:
  // NOLINTNEXTLINE(readability-inconsistent-declaration-parameter-name) -- Qt moc names parameters.
  void received(const QString& space, const QString& key, const QDBusVariant& value);
};
// NOLINTBEGIN(readability-convert-member-functions-to-static) -- QTest discovers instance slots.
class SettingsTests : public QObject {
  Q_OBJECT
 private slots:
  void ReadPublishesResolvedColorScheme() {
    SettingsPortalBackend backend(
        makeAppearance(QStringLiteral("holonight-day"), QStringLiteral("blue"), Holonight::ColorMode::Light));

    QCOMPARE(
        backend.Read(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme")).variant().toUInt(),
        2U);
  }

  void PublishesStandardAccessibilityDefaults() {
    SettingsPortalBackend backend(
        makeAppearance(QStringLiteral("holonight-dark"), QStringLiteral("blue"), Holonight::ColorMode::Dark));

    QCOMPARE(backend.Read(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("contrast")).variant().toUInt(),
             0U);
    QCOMPARE(
        backend.Read(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("reduced-motion")).variant().toUInt(),
        0U);

    const auto all = backend.ReadAll({QStringLiteral("org.freedesktop.appearance")});
    QCOMPARE(all.value(QStringLiteral("org.freedesktop.appearance"))
                 .value(QStringLiteral("contrast"))
                 .value<QDBusVariant>()
                 .variant()
                 .toUInt(),
             0U);
    QCOMPARE(all.value(QStringLiteral("org.freedesktop.appearance"))
                 .value(QStringLiteral("reduced-motion"))
                 .value<QDBusVariant>()
                 .variant()
                 .toUInt(),
             0U);
  }

  void ReadUsesInjectedResolvedColorMode() {
    SettingsPortalBackend backend(
        makeAppearance(QStringLiteral("holonight-light"), QStringLiteral("blue"), Holonight::ColorMode::Light));

    QCOMPARE(
        backend.Read(QStringLiteral("org.freedesktop.appearance"), QStringLiteral("color-scheme")).variant().toUInt(),
        2U);
  }

  void ReadAllPublishesAppearanceValues() {
    SettingsPortalBackend backend(
        makeAppearance(QStringLiteral("holonight-dark"), QStringLiteral("violet"), Holonight::ColorMode::Dark));
    const QMap<QString, QVariantMap> all = backend.ReadAll({QStringLiteral("org.freedesktop.appearance")});
    const QVariantMap appearance = all.value(QStringLiteral("org.freedesktop.appearance"));

    QCOMPARE(appearance.value(QStringLiteral("color-scheme")).value<QDBusVariant>().variant().toUInt(), 1U);
    const auto accent = appearance.value(QStringLiteral("accent-color"))
                            .value<QDBusVariant>()
                            .variant()
                            .value<SettingsPortalAccentColor>();
    QVERIFY(qAbs(accent.red - (0x9a / 255.0)) <= 0.001);
    QVERIFY(qAbs(accent.green - (0x8c / 255.0)) <= 0.001);
    QVERIFY(qAbs(accent.blue - (0xff / 255.0)) <= 0.001);
  }

  void DefaultAccentUsesSchemeNativeColor() {
    SettingsPortalBackend backend(
        makeAppearance(QStringLiteral("holonight-latte"), QStringLiteral("default"), Holonight::ColorMode::Light));
    const QMap<QString, QVariantMap> all = backend.ReadAll({QStringLiteral("org.freedesktop.appearance")});
    const QVariantMap appearance = all.value(QStringLiteral("org.freedesktop.appearance"));
    const auto accent = appearance.value(QStringLiteral("accent-color"))
                            .value<QDBusVariant>()
                            .variant()
                            .value<SettingsPortalAccentColor>();

    QVERIFY(qAbs(accent.red - (0x1e / 255.0)) <= 0.001);
    QVERIFY(qAbs(accent.green - (0x66 / 255.0)) <= 0.001);
    QVERIFY(qAbs(accent.blue - (0xf5 / 255.0)) <= 0.001);
  }

  void ApplyAppearanceEmitsOnlyChangedSystemFacingValues() {
    SettingsPortalBackend backend(
        makeAppearance(QStringLiteral("holonight-dark"), QStringLiteral("cyan"), Holonight::ColorMode::Dark));
    QSignalSpy changed_spy(&backend, &SettingsPortalBackend::SettingChanged);

    backend.applyAppearance(
        makeAppearance(QStringLiteral("holonight-light"), QStringLiteral("cyan"), Holonight::ColorMode::Light));

    QCOMPARE(changed_spy.count(), 2);
    QCOMPARE(changed_spy.takeFirst().at(1).toString(), QStringLiteral("color-scheme"));
    QCOMPARE(changed_spy.takeFirst().at(1).toString(), QStringLiteral("accent-color"));

    backend.applyAppearance(
        makeAppearance(QStringLiteral("holonight-light"), QStringLiteral("yellow"), Holonight::ColorMode::Light));

    QCOMPARE(changed_spy.count(), 1);
    QCOMPARE(changed_spy.takeFirst().at(1).toString(), QStringLiteral("accent-color"));
  }

  void namespaceFiltering() {
    SettingsPortalBackend backend(makeAppearance("holonight-dark", "blue", Holonight::ColorMode::Dark));
    QVERIFY(backend.ReadAll({"other"}).isEmpty());
    QCOMPARE(backend.ReadAll({}).size(), 1);
    QCOMPARE(backend.ReadAll({AppearanceNs, "other"}).size(), 1);
    QVERIFY(!backend.Read("other", "color-scheme").variant().isValid());
    QVERIFY(!backend.Read(AppearanceNs, "missing").variant().isValid());
  }
  void wireProtocol() {
    NoPresenter presenter;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    for (const auto* key : {"color-scheme", "contrast", "reduced-motion"}) {
      const auto reply = settingsCall("Read", {AppearanceNs, key});
      QCOMPARE(reply.signature(), "v");
      QCOMPARE(reply.arguments()[0].value<QDBusVariant>().variant().metaType(), QMetaType::fromType<uint>());
    }
    const auto accent = settingsCall("Read", {AppearanceNs, "accent-color"});
    QCOMPARE(accent.arguments()[0].value<QDBusVariant>().variant().value<QDBusArgument>().currentSignature(), "(ddd)");
    for (const auto& args : {QVariantList{AppearanceNs, "missing"}, QVariantList{"other", "color-scheme"}}) {
      QCOMPARE(settingsCall("Read", args).errorName(), "org.freedesktop.portal.Error.NotFound");
    }
    QCOMPARE(settingsCall("Read", {AppearanceNs, "missing"}).errorMessage(), "Requested setting not found");
    QCOMPARE(settingsCall("Read", {"other", "color-scheme"}).errorMessage(), "Requested setting namespace not found");
    QCOMPARE(settingsCall("Read", {AppearanceNs}).errorName(), "org.freedesktop.DBus.Error.InvalidArgs");
    const auto all = settingsCall("ReadAll", {QStringList{}});
    QCOMPARE(all.signature(), "a{sa{sv}}");
    QCOMPARE((decode<QMap<QString, QVariantMap>>(all.arguments()[0]).value(AppearanceNs).size()), 4);
    const auto filtered = settingsCall("ReadAll", {QStringList{"other"}});
    QVERIFY((decode<QMap<QString, QVariantMap>>(filtered.arguments()[0]).isEmpty()));
  }
  void registrationRollback() {
    const auto bus = QDBusConnection::sessionBus();
    NoPresenter presenter;
    {
      Backend running(bus, &presenter);
      QVERIFY(running.start());
      {
        Backend collision(bus, &presenter);
        QVERIFY(!collision.start());
      }
      QCOMPARE(settingsCall("Read", {AppearanceNs, "contrast"}).signature(), "v");
    }
    auto other = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "settings-owner");
    QVERIFY(other.registerService(BusName));
    {
      Backend collision(bus, &presenter);
      QVERIFY(!collision.start());
    }
    QVERIFY(other.interface()->isServiceRegistered(BusName).value());
    // The failed start must release its object, and leave the other connection's name alone.
    other.unregisterService(BusName);
    Backend recovered(bus, &presenter);
    QVERIFY(recovered.start());
    recovered.shutdown();
    QVERIFY(!bus.interface()->isServiceRegistered(BusName).value());
    QDBusConnection::disconnectFromBus("settings-owner");
  }
  void watchedConfiguration() {
    QTemporaryDir directory;
    const auto path = directory.filePath("appearance.toml");
    qputenv("HOLONIGHT_APPEARANCE_FILE", path.toUtf8());
    const auto reset = qScopeGuard([] { qunsetenv("HOLONIGHT_APPEARANCE_FILE"); });
    QVERIFY(replace(path, "invalid = ["));
    NoPresenter presenter;
    Backend backend(QDBusConnection::sessionBus(), &presenter);
    QVERIFY(backend.start());
    const auto readScheme = [] {
      return settingsCall("Read", {AppearanceNs, "color-scheme"})
          .arguments()[0]
          .value<QDBusVariant>()
          .variant()
          .toUInt();
    };
    QCOMPARE(readScheme(), 1U);  // Startup defaults survive invalid configuration.
    SettingsSignals receiver;
    QSignalSpy changed(&receiver, &SettingsSignals::received);
    QVERIFY(QDBusConnection::sessionBus().connect(BusName, DesktopPath, "org.freedesktop.impl.portal.Settings",
                                                  "SettingChanged", &receiver,
                                                  SLOT(changed(QString, QString, QDBusVariant))));
    QFile fixture(QFINDTESTDATA("appearance.toml"));
    QVERIFY(fixture.open(QIODevice::ReadOnly));
    const auto dark = fixture.readAll();
    auto light = dark;
    light.replace("holonight-dark", "holonight-day");
    light.replace("blue", "violet");
    QVERIFY(replace(path, light));
    QTRY_COMPARE_WITH_TIMEOUT(readScheme(), 2U, 3000);
    QTRY_COMPARE(changed.size(), 2);
    changed.clear();
    QVERIFY(replace(path, light + "\n# same effective values\n"));
    QTest::qWait(250);
    QCOMPARE(changed.size(), 0);
    QVERIFY(replace(path, QByteArray(light).replace("ui_size = 14", "ui_size = 15")));
    QTest::qWait(250);
    QCOMPARE(changed.size(), 0);
    const auto accent = settingsCall("Read", {AppearanceNs, "accent-color"});
    const auto before = decode<SettingsPortalAccentColor>(accent.arguments()[0].value<QDBusVariant>().variant());
    QVERIFY(replace(path, "invalid = ["));
    QTest::qWait(250);
    QCOMPARE(readScheme(), 2U);
    QCOMPARE(changed.size(), 0);
    const auto after = decode<SettingsPortalAccentColor>(
        settingsCall("Read", {AppearanceNs, "accent-color"}).arguments()[0].value<QDBusVariant>().variant());
    QCOMPARE(after.red, before.red);
    QVERIFY(replace(path, dark));
    QTRY_COMPARE_WITH_TIMEOUT(readScheme(), 1U, 3000);
  }
};
// NOLINTEND(readability-convert-member-functions-to-static)
QTEST_GUILESS_MAIN(SettingsTests)
#include "test_settings.moc"
