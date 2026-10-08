#include "wayland_parent.h"

#include "xdg-foreign-client.h"

#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QWindow>
#include <QtGui/qpa/qplatformwindow_p.h>

#include <cstring>
#include <wayland-client.h>

namespace Portal {
struct WaylandParent::Impl {
  Impl() = default;
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;
  wl_display* display = nullptr;
  wl_event_queue* queue = nullptr;
  wl_display* wrapper = nullptr;
  wl_registry* registry = nullptr;
  zxdg_importer_v2* importer = nullptr;
  zxdg_imported_v2* imported = nullptr;
  QPointer<QWindow> window;
  QByteArray handle;
  QTimer dispatch;
  bool destroyed = false;
  ~Impl() {
    dispatch.stop();
    if (imported != nullptr) {
      zxdg_imported_v2_destroy(imported);
    }
    if (importer != nullptr) {
      zxdg_importer_v2_destroy(importer);
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
  void import() {
    if (destroyed || (imported != nullptr) || (importer == nullptr) || !window) {
      return;
    }
    auto* native = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    auto* surface = (native != nullptr) ? native->surface() : nullptr;
    if (surface == nullptr) {
      return;
    }
    imported = zxdg_importer_v2_import_toplevel(importer, handle.constData());
    static constexpr zxdg_imported_v2_listener listener{.destroyed = [](void* data, zxdg_imported_v2*) {
      auto* self = static_cast<Impl*>(data);
      self->destroyed = true;
      zxdg_imported_v2_destroy(self->imported);
      self->imported = nullptr;
    }};
    zxdg_imported_v2_add_listener(imported, &listener, this);
    zxdg_imported_v2_set_parent_of(imported, surface);
    wl_display_flush(display);
  }
};
WaylandParent::WaylandParent(QWindow* window, const QString& parent, QObject* owner)
    : QObject(owner), impl_(std::make_unique<Impl>()) {
  auto* app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
  if (!parent.startsWith("wayland:") || parent.size() == 8 || (app == nullptr) || (app->display() == nullptr)) {
    return;
  }
  auto& state = *impl_;
  state.display = app->display();
  state.window = window;
  state.handle = parent.mid(8).toUtf8();
  if (state.handle.contains('\0')) {
    return;
  }
  state.queue = wl_display_create_queue(state.display);
  state.wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(state.display));
  if ((state.queue == nullptr) || (state.wrapper == nullptr)) {
    return;
  }
  // wl_display and wl_proxy share libwayland's C object representation.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  wl_proxy_set_queue(reinterpret_cast<wl_proxy*>(state.wrapper), state.queue);
  state.registry = wl_display_get_registry(state.wrapper);
  static constexpr wl_registry_listener listener{
      .global =
          [](void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t) {
            auto* self = static_cast<Impl*>(data);
            if (!self->importer && std::strcmp(interface, zxdg_importer_v2_interface.name) == 0) {
              self->importer =
                  static_cast<zxdg_importer_v2*>(wl_registry_bind(registry, name, &zxdg_importer_v2_interface, 1));
              self->import();
            }
          },
      .global_remove = [](void*, wl_registry*, uint32_t) {}};
  wl_registry_add_listener(state.registry, &listener, &state);
  // Qt owns reading the shared display. Dispatch only already-read private-queue events
  // from the event loop; never roundtrip or run another interaction loop.
  connect(&state.dispatch, &QTimer::timeout, this, [this] {
    if (wl_display_dispatch_queue_pending(impl_->display, impl_->queue) < 0) {
      impl_->dispatch.stop();
    }
    impl_->import();
  });
  state.dispatch.start(10);
  wl_display_flush(state.display);
}
WaylandParent::~WaylandParent() = default;
}  // namespace Portal
