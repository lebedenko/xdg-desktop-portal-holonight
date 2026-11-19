#include "backend.h"
#include "screenshot.h"
#include "wire.h"

#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusPendingCall>
#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QUrl>

using namespace Portal;

namespace {

constexpr auto Interface = "org.freedesktop.impl.portal.Screenshot";
constexpr auto BrokerName = "org.freedesktop.portal.Desktop";
constexpr auto Handle = "/org/freedesktop/portal/desktop/request/test/first";

class Desktop final : public QDBusVirtualObject {
 public:
  using Target = ScreenshotPortal::Target;

  std::optional<Target> pendingApproval;
  std::optional<Target> startedCapture;
  std::function<void(bool)> approve;
  std::function<void(const QImage&)> image;
  std::function<void(const QColor&)> color;
  ScreenshotPortal portal{[this](Target target, auto callback) {
                            pendingApproval = target;
                            approve = std::move(callback);
                          },
                          [this](Target target, auto imageCallback, auto colorCallback) {
                            startedCapture = target;
                            image = std::move(imageCallback);
                            color = std::move(colorCallback);
                          }};

  QString introspect(const QString&) const override { return ScreenshotPortal::introspection(); }
  bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override {
    return portal.handleMessage(message, connection, false);
  }
};

QDBusPendingCall call(const QDBusConnection& client, const QString& method, const QVariantMap& options,
                      const QString& handle = Handle) {
  auto message = QDBusMessage::createMethodCall(BusName, DesktopPath, Interface, method);
  message << QVariant::fromValue(QDBusObjectPath(handle)) << QString("org.example.Test") << QString() << options;
  return client.asyncCall(message);
}

QImage pixels() {
  QImage image(2, 2, QImage::Format_RGB32);
  image.fill(Qt::red);
  return image;
}

QVariantMap results(const QDBusPendingCall& pending) {
  return decode<QVariantMap>(pending.reply().arguments().value(1));
}

}  // namespace

class ScreenshotTest final : public QObject {
  Q_OBJECT

 private slots:
  void initTestCase() {
    QVERIFY(runtime_.isValid());
    qputenv("XDG_RUNTIME_DIR", runtime_.path().toUtf8());
    qDBusRegisterMetaType<SettingsPortalAccentColor>();
    QVERIFY(server_.registerService(BusName));
    QVERIFY(broker_.registerService(BrokerName));
  }

  void init() {
    desktop_ = std::make_unique<Desktop>();
    QVERIFY(server_.registerVirtualObject(DesktopPath, desktop_.get()));
  }

  void cleanup() {
    desktop_.reset();
    server_.unregisterObject(DesktopPath);
  }

  void protocolAndAuthorization() {
    auto introspect = serverCall(DesktopPath, "org.freedesktop.DBus.Introspectable", "Introspect");
    QTRY_VERIFY(introspect.isFinished());
    const auto xml = introspect.reply().arguments().value(0).toString();
    QVERIFY(xml.contains("method name=\"Screenshot\""));
    QVERIFY(xml.contains("method name=\"PickColor\""));

    auto properties = serverCall(DesktopPath, "org.freedesktop.DBus.Properties", "GetAll", {QString(Interface)});
    QTRY_VERIFY(properties.isFinished());
    QCOMPARE(properties.reply().signature(), "a{sv}");
    const auto values = decode<QVariantMap>(properties.reply().arguments().value(0));
    QCOMPARE(values.value("version"), QVariant(3U));
    QCOMPARE(values.value("AvailableTargets"), QVariant(5U));

    for (const auto* method : {"Screenshot", "PickColor"}) {
      auto unauthorized = call(client_, method, {{"permission_store_checked", true}});
      QTRY_VERIFY(unauthorized.isFinished());
      QCOMPARE(unauthorized.reply().errorName(), "org.freedesktop.DBus.Error.AccessDenied");
    }
    auto malformed = serverCall(DesktopPath, Interface, "Screenshot", {QString("invalid")}, &broker_);
    QTRY_VERIFY(malformed.isFinished());
    QCOMPARE(malformed.reply().errorName(), "org.freedesktop.DBus.Error.InvalidArgs");
    QVERIFY(!desktop_->pendingApproval && !desktop_->startedCapture);
  }

  void targetPolicy_data() {
    QTest::addColumn<QVariantMap>("options");
    QTest::addColumn<int>("before");
    QTest::addColumn<int>("after");
    using Target = Desktop::Target;
    const QVariant checked(true);
    QTest::newRow("desktop approval") << QVariantMap{} << int(Target::Desktop) << int(Target::Desktop);
    QTest::newRow("checked desktop") << QVariantMap{{"permission_store_checked", checked}} << -1
                                     << int(Target::Desktop);
    QTest::newRow("interactive choice") << QVariantMap{{"interactive", true}, {"permission_store_checked", checked}}
                                        << int(Target::Choice) << int(Target::Area);
    QTest::newRow("explicit desktop") << QVariantMap{{"interactive", true}, {"target", 1U}} << int(Target::Desktop)
                                      << int(Target::Desktop);
    QTest::newRow("explicit area") << QVariantMap{{"target", 4U}, {"permission_store_checked", checked}}
                                   << int(Target::Area) << int(Target::Area);
    QTest::newRow("unsupported target") << QVariantMap{{"target", 2U}} << -1 << -1;
    QTest::newRow("multiple targets") << QVariantMap{{"target", 5U}} << -1 << -1;
  }

  void targetPolicy() {
    QFETCH(QVariantMap, options);
    QFETCH(int, before);
    QFETCH(int, after);
    auto pending = call(broker_, "Screenshot", options);
    if (after < 0) {
      QTRY_VERIFY(pending.isFinished());
      QCOMPARE(pending.reply().arguments().value(0).toUInt(), 2U);
      QVERIFY(!desktop_->pendingApproval && !desktop_->startedCapture);
      return;
    }
    if (before >= 0) {
      QTRY_VERIFY(desktop_->pendingApproval.has_value());
      QCOMPARE(int(*desktop_->pendingApproval), before);
      QVERIFY(!desktop_->startedCapture);
      desktop_->approve(true);
    }
    QTRY_VERIFY(desktop_->startedCapture.has_value());
    QCOMPARE(int(*desktop_->startedCapture), after);
    desktop_->image(pixels());
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().signature(), "ua{sv}");
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), 0U);
    const auto uri = results(pending).value("uri");
    QCOMPARE(uri.metaType(), QMetaType::fromType<QString>());
    const QFileInfo png(QUrl(uri.toString()).toLocalFile());
    QVERIFY(png.exists());
    QCOMPARE(png.dir().path().section('/', 0, -2), runtime_.path());
    const QFileInfo directory(png.absolutePath());
    QVERIFY(!(directory.permissions() & (QFile::ReadGroup | QFile::ReadOther)));
    QCOMPARE(QImage(png.absoluteFilePath()).pixelColor(0, 0), QColor(Qt::red));
    QVERIFY(QFile::remove(png.absoluteFilePath()));
    QVERIFY(QDir().rmdir(png.absolutePath()));
  }

  void colorBusyCloseAndLateCallbacks() {
    auto pending = call(broker_, "PickColor", {{"permission_store_checked", true}});
    QTRY_COMPARE(desktop_->pendingApproval, Desktop::Target::Color);
    QVERIFY(!desktop_->startedCapture);
    auto busy = call(broker_, "Screenshot", {}, "/org/freedesktop/portal/desktop/request/test/second");
    QTRY_VERIFY(busy.isFinished());
    QCOMPARE(busy.reply().arguments().value(0).toUInt(), 2U);

    auto unauthorized = serverCall(Handle, "org.freedesktop.impl.portal.Request", "Close");
    QTRY_VERIFY(unauthorized.isFinished());
    QCOMPARE(unauthorized.reply().errorName(), "org.freedesktop.DBus.Error.AccessDenied");
    auto close = serverCall(Handle, "org.freedesktop.impl.portal.Request", "Close", {}, &broker_);
    QTRY_VERIFY(close.isFinished() && pending.isFinished());
    QCOMPARE(close.reply().type(), QDBusMessage::ReplyMessage);
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), 1U);
    desktop_->approve(false);
    QVERIFY(!desktop_->startedCapture);

    pending = call(broker_, "PickColor", {});
    QTRY_VERIFY(!pending.isFinished() && desktop_->pendingApproval.has_value());
    QTRY_VERIFY([&] {
      auto registered = serverCall(Handle, "org.freedesktop.DBus.Introspectable", "Introspect");
      return QTest::qWaitFor([&] { return registered.isFinished(); }) &&
             registered.reply().arguments().value(0).toString().contains("impl.portal.Request");
    }());
    desktop_->approve(false);
    QTRY_COMPARE(desktop_->startedCapture, Desktop::Target::Color);
    desktop_->color(Qt::green);
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), 0U);
    const auto color = results(pending).value("color").value<QDBusArgument>();
    QCOMPARE(color.currentSignature(), "(ddd)");
    const auto rgb = qdbus_cast<SettingsPortalAccentColor>(color);
    QCOMPARE(rgb.red, 0.0);
    QCOMPARE(rgb.green, 1.0);
    QCOMPARE(rgb.blue, 0.0);
  }

  void shutdownDuringAcquisition() {
    auto pending = call(broker_, "Screenshot", {{"permission_store_checked", true}});
    QTRY_COMPARE(desktop_->startedCapture, Desktop::Target::Desktop);
    desktop_->portal.shutdown();
    desktop_->image(pixels());
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), 2U);
    QVERIFY(results(pending).isEmpty());
    QTRY_VERIFY(QDir(runtime_.path()).isEmpty());
  }

  void shutdownWhileEncodingKeepsNoPng() {
    auto pending = call(broker_, "Screenshot", {{"permission_store_checked", true}});
    QTRY_COMPARE(desktop_->startedCapture, Desktop::Target::Desktop);
    desktop_->image(pixels());
    desktop_->portal.shutdown();
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), 2U);
    QVERIFY(results(pending).isEmpty());
    QTRY_VERIFY(QDir(runtime_.path()).isEmpty());
  }

  void ownerLossKeepsNoPng() {
    auto pending = call(broker_, "Screenshot", {{"permission_store_checked", true}});
    QTRY_COMPARE(desktop_->startedCapture, Desktop::Target::Desktop);
    QVERIFY(broker_.unregisterService(BrokerName));
    desktop_->image(pixels());
    QTRY_VERIFY(pending.isFinished());
    QCOMPARE(pending.reply().arguments().value(0).toUInt(), 1U);
    QVERIFY(results(pending).isEmpty());
    desktop_->image(pixels());
    QTRY_VERIFY(QDir(runtime_.path()).isEmpty());
    QVERIFY(broker_.registerService(BrokerName));
  }

 private:
  QDBusPendingCall serverCall(const QString& path, const QString& interface, const QString& member,
                              const QVariantList& args = {}, const QDBusConnection* client = nullptr) {
    auto message = QDBusMessage::createMethodCall(BusName, path, interface, member);
    message.setArguments(args);
    return (client ? *client : client_).asyncCall(message);
  }

  QTemporaryDir runtime_;
  QDBusConnection server_ = QDBusConnection::sessionBus();
  QDBusConnection broker_ = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "screenshot-broker");
  QDBusConnection client_ = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "screenshot-client");
  std::unique_ptr<Desktop> desktop_;
};

QTEST_GUILESS_MAIN(ScreenshotTest)
#include "test_screenshot.moc"
