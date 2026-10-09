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
  WaylandParent* owner = nullptr;
  wl_callback* sync = nullptr;
  bool import_requested = false;
  bool destroyed = false;
  ~Impl() {
    dispatch.stop();
    if (sync != nullptr) {
      wl_callback_destroy(sync);
    }
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
  void synchronize() {
    // The first barrier finishes registry discovery; the second validates the import
    // before show() can create and commit the chooser shell role.
    sync = wl_display_sync(wrapper);
    static constexpr wl_callback_listener listener{.done = [](void* data, wl_callback* callback, uint32_t) {
      auto* self = static_cast<Impl*>(data);
      wl_callback_destroy(callback);
      self->sync = nullptr;
      if (self->import_requested || self->importer == nullptr) {
        self->owner->complete(self->imported != nullptr);
        return;
      }
      self->import_requested = true;
      self->imported = zxdg_importer_v2_import_toplevel(self->importer, self->handle.constData());
      static constexpr zxdg_imported_v2_listener imported_listener{
          .destroyed = [](void* imported_data, zxdg_imported_v2* imported) {
            auto* state = static_cast<Impl*>(imported_data);
            state->destroyed = true;
            zxdg_imported_v2_destroy(imported);
            state->imported = nullptr;
          }};
      zxdg_imported_v2_add_listener(self->imported, &imported_listener, self);
      self->synchronize();
    }};
    wl_callback_add_listener(sync, &listener, this);
    wl_display_flush(display);
  }
  void apply() const {
    if (destroyed || imported == nullptr || !window) {
      return;
    }
    auto* native = window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
    auto* surface = native != nullptr ? native->surface() : nullptr;
    if (surface != nullptr) {
      zxdg_imported_v2_set_parent_of(imported, surface);
      wl_display_flush(display);
    }
  }
};
WaylandParent::WaylandParent(QWindow* window, const QString& parent, QObject* owner) : WaylandParent(owner) {
  impl_->window = window;
  impl_->handle = parent.startsWith("wayland:") ? parent.mid(8).toUtf8() : QByteArray{};
}
WaylandParent::WaylandParent(QObject* owner) : QObject(owner), impl_(std::make_unique<Impl>()) {
  impl_->owner = this;
  deadline_.setSingleShot(true);
  connect(&deadline_, &QTimer::timeout, this, [this] {
    impl_->dispatch.stop();
    complete();
  });
}
void WaylandParent::prepare() {
  if (started_) {
    return;
  }
  started_ = true;
  deadline_.start(1000);
  prepareNative();
}
void WaylandParent::complete(bool parent_ready) {
  if (!completed_) {
    completed_ = true;
    parent_ready_ = parent_ready;
    deadline_.stop();
    emit prepared();
  }
}
void WaylandParent::onSurfaceRoleCreated() {
  if (parent_ready_) {
    applyParent();
  }
}
void WaylandParent::applyParent() { impl_->apply(); }
void WaylandParent::prepareNative() {
  auto* app = qGuiApp ? qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>() : nullptr;
  if (impl_->handle.isEmpty() || impl_->handle.contains('\0') || app == nullptr || app->display() == nullptr) {
    complete();
    return;
  }
  auto& state = *impl_;
  state.display = app->display();
  auto* native = state.window->nativeInterface<QNativeInterface::Private::QWaylandWindow>();
  if (native == nullptr) {
    complete();
    return;
  }
  watchRole(native);
  state.queue = wl_display_create_queue(state.display);
  state.wrapper = static_cast<wl_display*>(wl_proxy_create_wrapper(state.display));
  if ((state.queue == nullptr) || (state.wrapper == nullptr)) {
    complete();
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
            }
          },
      .global_remove = [](void*, wl_registry*, uint32_t) {}};
  wl_registry_add_listener(state.registry, &listener, &state);
  // Qt owns reading the shared display. Dispatch only already-read private-queue events
  // from the event loop; never roundtrip or run another interaction loop.
  connect(&state.dispatch, &QTimer::timeout, this, [this] {
    if (wl_display_dispatch_queue_pending(impl_->display, impl_->queue) < 0) {
      impl_->dispatch.stop();
      complete();
    }
  });
  state.dispatch.start(10);
  state.synchronize();
}
WaylandParent::~WaylandParent() = default;
}  // namespace Portal
