#include "backend.h"
#include "presenter.h"
#include "xdg-foreign-client.h"

#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDir>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QWindow>
#include <QtGui/qpa/qplatformwindow_p.h>

#include <cstring>
#include <wayland-client.h>

// A manual native host. It never moves the pointer, activates a window or synthesizes input.
struct Probe {
  Probe() = default;
  Probe(const Probe&) = delete;
  Probe& operator=(const Probe&) = delete;
  Probe(Probe&&) = delete;
  Probe& operator=(Probe&&) = delete;
  QQuickWindow parent;
  Portal::QmlPresenter presenter;
  QDBusConnection broker = QDBusConnection::connectToBus(QDBusConnection::SessionBus, "native-broker");
  Portal::Backend backend{QDBusConnection::sessionBus(), &presenter};
  wl_display* display = nullptr;
  wl_event_queue* queue = nullptr;
  wl_display* wrapper = nullptr;
  wl_registry* registry = nullptr;
  zxdg_exporter_v2* exporter = nullptr;
  zxdg_exported_v2* exported = nullptr;
  wl_callback* registry_sync = nullptr;
  QTimer dispatch;
  QString mode;
  bool modal = true;
  bool started = false;
  ~Probe() {
    dispatch.stop();
    backend.shutdown();
    if (registry_sync != nullptr) {
      wl_callback_destroy(registry_sync);
    }
    if (exported != nullptr) {
      zxdg_exported_v2_destroy(exported);
    }
    if (exporter != nullptr) {
      zxdg_exporter_v2_destroy(exporter);
    }
    if (registry != nullptr) {
      wl_registry_destroy(registry);
    }
    if (wrapper != nullptr) {
      wl_proxy_wrapper_destroy(wrapper);
    }
    if (queue != nullptr) {
      wl_event_queue_destroy(queue);
    }
    if (display != nullptr) {
      wl_display_flush(display);
    }
  }
  void request(const QString& handle) {
    if (started) {
      return;
    }
    started = true;
    const char* method = "OpenFile";
    if (mode == "save") {
      method = "SaveFile";
    } else if (mode == "many") {
      method = "SaveFiles";
    }
    QVariantMap options{{"current_folder", QFile::encodeName(QDir::homePath()) + '\0'},
                        {"modal", modal},
                        {"multiple", mode == "multi"},
                        {"directory", mode == "directory"},
                        {"current_name", "portal-test.txt"},
                        {"files", QVariant::fromValue(Portal::Names{"portal-test.txt", "portal-test.txt"})}};
    auto message = QDBusMessage::createMethodCall(Portal::BusName, Portal::DesktopPath,
                                                  "org.freedesktop.impl.portal.FileChooser", method);
    message.setArguments({QVariant::fromValue(QDBusObjectPath("/test/native")), "native.probe", handle,
                          "Manual HoloNight chooser", options});
    auto* watcher = new QDBusPendingCallWatcher(broker.asyncCall(message), &parent);
    QObject::connect(watcher, &QDBusPendingCallWatcher::finished, &parent, [watcher] {
      QDBusPendingReply<uint, QVariantMap> reply(*watcher);
      if (reply.isError()) {
        qWarning() << reply.error();
      } else {
        qInfo() << "Response:" << reply.argumentAt<0>() << "Results:" << reply.argumentAt<1>();
      }
      qInfo() << "Close the parent window to finish. No files were created.";
      watcher->deleteLater();
    });
  }
  bool start() {
    if (!backend.start()) {
      return false;
    }
    parent.setTitle("HoloNight parent probe — close to finish");
    parent.setColor(QColor("#2b2e36"));
    parent.resize(450, 220);
    parent.show();
    auto* native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if ((native == nullptr) || (native->display() == nullptr) || mode == "unparented") {
      request({});
      return true;
    }
    display = native->display();
    queue = wl_display_create_queue(display);
    wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(display));
    if ((queue == nullptr) || (wrapper == nullptr)) {
      return false;
    }
    // libwayland exposes displays as proxies through its C object representation.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(wrapper), queue);
    registry = wl_display_get_registry(wrapper);
    static constexpr wl_registry_listener listener{
        .global =
            [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t) {
              auto* self = static_cast<Probe*>(data);
              if (!self->exporter && std::strcmp(interface, zxdg_exporter_v2_interface.name) == 0) {
                self->exporter =
                    static_cast<zxdg_exporter_v2*>(wl_registry_bind(registry, name, &zxdg_exporter_v2_interface, 1));
              }
            },
        .global_remove = [](void*, wl_registry*, uint32_t) {}};
    wl_registry_add_listener(registry, &listener, this);
    registry_sync = wl_display_sync(wrapper);
    static constexpr wl_callback_listener syncListener{.done = [](void* data, wl_callback* callback, uint32_t) {
      auto* self = static_cast<Probe*>(data);
      wl_callback_destroy(callback);
      self->registry_sync = nullptr;
      if (self->exporter == nullptr) {
        qInfo() << "xdg-foreign export is unavailable; opening an ordinary chooser.";
        self->request({});
      }
    }};
    wl_callback_add_listener(registry_sync, &syncListener, this);
    QObject::connect(&dispatch, &QTimer::timeout, &parent, [this] {
      wl_display_dispatch_queue_pending(display, queue);
      if (!exporter || exported) {
        return;
      }
      auto* nativeWindow = parent.nativeInterface<QNativeInterface::Private::QWaylandWindow>();
      if (!nativeWindow || !nativeWindow->surface()) {
        return;
      }
      exported = zxdg_exporter_v2_export_toplevel(exporter, nativeWindow->surface());
      static constexpr zxdg_exported_v2_listener handleListener{
          .handle = [](void* data, zxdg_exported_v2*, const char* handle) {
            auto* self = static_cast<Probe*>(data);
            qInfo() << "Exported native parent. Verify compositor parenting and modality manually.";
            self->request("wayland:" + QString::fromUtf8(handle));
          }};
      zxdg_exported_v2_add_listener(exported, &handleListener, this);
      wl_display_flush(display);
    });
    dispatch.start(10);
    wl_display_flush(display);
    return true;
  }
};
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QQuickStyle::setStyle("Holonight");
  Probe probe;
  probe.mode = QGuiApplication::arguments().value(1, "open");
  probe.modal = !QGuiApplication::arguments().contains("--nonmodal");
  if (!probe.start()) {
    return 1;
  }
  return QGuiApplication::exec();
}
