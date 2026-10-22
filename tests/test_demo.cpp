#include "demo_controller.h"

#include <QDBusArgument>
#include <QDBusObjectPath>
#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QTest>

class DemoService : public QDBusVirtualObject {
 public:
  QList<QDBusMessage> calls;
  [[nodiscard]] QString introspect(const QString& /*path*/) const override { return {}; }
  bool handleMessage(const QDBusMessage& message, const QDBusConnection& /*connection*/) override {
    message.setDelayedReply(true);
    calls.append(message);
    return true;
  }
};
// NOLINTBEGIN(readability-convert-member-functions-to-static)
class DemoTests : public QObject {
  Q_OBJECT
 private slots:
  void requestsAndResults() {
    auto bus = QDBusConnection::sessionBus();
    DemoService service;
    QVERIFY(bus.registerService(Portal::BusName));
    QVERIFY(bus.registerVirtualObject(Portal::DesktopPath, &service));
    auto client = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "demo-test-client");
    DemoController demo(client);
    QVERIFY(!demo.enabled());
    demo.request(0);
    QCOMPARE(service.calls.size(), 0);
    demo.ready("wayland:test");
    QStringList paths;
    const QStringList methods{"OpenFile", "OpenFile", "OpenFile", "OpenFile", "SaveFile", "SaveFiles"};
    for (int picker = 0; picker < 6; ++picker) {
      demo.request(picker);
      QVERIFY(!demo.enabled());
      demo.request(picker);
      QTRY_COMPARE(service.calls.size(), picker + 1);
      const auto call = service.calls.last();
      QCOMPARE(call.member(), methods.at(picker));
      QCOMPARE(call.arguments().at(2).toString(), "wayland:test");
      const auto path = call.arguments().at(0).value<QDBusObjectPath>().path();
      QVERIFY(!paths.contains(path));
      paths.append(path);
      const auto options = qdbus_cast<QVariantMap>(call.arguments().at(4));
      QCOMPARE(options.value("current_folder").toByteArray(), QFile::encodeName(QDir::homePath()) + '\0');
      QVERIFY(options.value("modal").toBool());
      if (picker < 4) {
        QCOMPARE(options.value("multiple").toBool(), picker == 1 || picker == 3);
        QCOMPARE(options.value("directory").toBool(), picker >= 2);
      } else if (picker == 4) {
        QCOMPARE(options.value("current_name").toString(), "portal-demo.txt");
      } else {
        QCOMPARE(qdbus_cast<Portal::Names>(options.value("files")),
                 (Portal::Names{"portal-demo-one.txt", "portal-demo-two.txt"}));
      }
      if (picker == 5) {
        QVERIFY(bus.send(call.createErrorReply("org.test.Failed", "Demo error detail")));
      } else {
        uint code = 0;
        if (picker == 2) {
          code = 1;
        }
        if (picker == 3) {
          code = 2;
        }
        const QVariantMap results{{"uris", QStringList{"file:///one", "file:///two"}}};
        QVERIFY(bus.send(call.createReply({code, results})));
      }
      QTRY_VERIFY(demo.enabled());
    }
    QVERIFY(demo.log().contains("accepted (response 0)"));
    QVERIFY(demo.log().contains("cancelled (response 1)"));
    QVERIFY(demo.log().contains("failed (response 2)"));
    QVERIFY(demo.log().contains("file:///one"));
    QVERIFY(demo.log().contains("file:///two"));
    QVERIFY(demo.log().contains("org.test.Failed: Demo error detail"));
    demo.request(0);
    QTRY_COMPARE(service.calls.size(), 7);
    QVERIFY(!paths.contains(service.calls.last().arguments().at(0).value<QDBusObjectPath>().path()));
    QVERIFY(bus.send(service.calls.last().createReply({1U, QVariantMap{}})));
    QTRY_VERIFY(demo.enabled());
    demo.fail("Backend unavailable");
    QVERIFY(!demo.enabled());
    QVERIFY(demo.log().contains("Backend unavailable"));
    bus.unregisterObject(Portal::DesktopPath);
    bus.unregisterService(Portal::BusName);
  }
  void qmlBindings() {
    DemoController demo(QDBusConnection::sessionBus());
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/HolonightPortal/Demo.qml"));
    QScopedPointer<QObject> view(component.createWithInitialProperties({{"controller", QVariant::fromValue(&demo)}}));
    QVERIFY2(view, qPrintable(component.errorString()));
    auto* root = qobject_cast<QQuickItem*>(view.data());
    root->setWidth(800);
    root->setHeight(560);
    // Repeater delegates are visual children rather than QObject children.
    QList<QQuickItem*> buttons;
    const auto visit = [&buttons](auto&& self, QQuickItem* item) -> void {
      if (item->objectName().startsWith("picker")) {
        buttons.append(item);
      }
      for (auto* child : item->childItems()) {
        self(self, child);
      }
    };
    visit(visit, root);
    QCOMPARE(buttons.size(), 6);
    for (auto* button : buttons) {
      QVERIFY(!button->isEnabled());
    }
    demo.ready({});
    for (auto* button : buttons) {
      QTRY_VERIFY(button->isEnabled());
    }
    QVERIFY(view->findChild<QObject*>("activityLog"));
    demo.request(0);
    for (auto* button : buttons) {
      QVERIFY(!button->isEnabled());
    }
    QTRY_VERIFY(demo.enabled());
    demo.fail("startup failure");
    for (auto* button : buttons) {
      QVERIFY(!button->isEnabled());
    }
  }
};
// NOLINTEND(readability-convert-member-functions-to-static)
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  Portal::registerWireTypes();
  QQuickStyle::setStyle("Holonight");
  DemoTests tests;
  return QTest::qExec(&tests, argc, argv);
}
#include "test_demo.moc"
