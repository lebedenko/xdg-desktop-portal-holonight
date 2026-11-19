#include "screenshot.h"

#include "backend.h"
#include "filesystem.h"
#include "wayland_parent.h"
#include "wire.h"

#include <QDBusConnectionInterface>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QQuickView>
#include <QScreen>
#include <QTemporaryDir>
#include <QtConcurrentRun>

#include <algorithm>
#include <capture_controller.h>
#include <functional>
#include <grim_acquisition.h>
#include <png_encoder.h>
#include <region_overlay.h>
#include <utility>
#include <wayland-client.h>

static void initializeApprovalResource() { Q_INIT_RESOURCE(approval); }

namespace Portal {
namespace {

constexpr auto Interface = "org.freedesktop.impl.portal.Screenshot";
constexpr auto Broker = "org.freedesktop.portal.Desktop";
constexpr auto RequestInterface = "org.freedesktop.impl.portal.Request";

using Target = ScreenshotPortal::Target;

struct CaptureOptions {
  Target target;
  bool approved;
  bool modal;
};

std::optional<CaptureOptions> captureOptions(bool pickColor, const QVariantMap& options) {
  const bool interactive =
      options.value("interactive").metaType() == QMetaType::fromType<bool>() && options.value("interactive").toBool();
  Target target = interactive ? Target::Choice : Target::Desktop;
  if (pickColor) {
    return CaptureOptions{Target::Color, false, true};
  }
  if (options.contains("target")) {
    const auto value = options.value("target");
    if (value.metaType() != QMetaType::fromType<uint>() || (value.toUInt() != 1 && value.toUInt() != 4)) {
      return std::nullopt;
    }
    target = value.toUInt() == 1 ? Target::Desktop : Target::Area;
  }
  const auto checked = options.value("permission_store_checked");
  const auto modal = options.value("modal");
  return CaptureOptions{target,
                        target == Target::Desktop && !interactive &&
                            checked.metaType() == QMetaType::fromType<bool>() && checked.toBool(),
                        modal.metaType() != QMetaType::fromType<bool>() || modal.toBool()};
}

bool isBroker(const QDBusConnection& connection, const QString& sender) {
  if (sender.isEmpty() || !connection.interface()) {
    return false;
  }
  const auto owner = connection.interface()->serviceOwner(Broker);
  return owner.isValid() && owner.value() == sender;
}

bool reply(const QDBusConnection& connection, const QDBusMessage& call, uint response,
           const QVariantMap& results = {}) {
  return connection.send(call.createReply({QVariant::fromValue(response), results}));
}

// The private event queue observes a barrier ordered after Qt destroys the dialog surface.
class DisplayBarrier final : public QObject {
 public:
  DisplayBarrier(QObject* parent, std::function<void(bool)> done) : QObject(parent), done_(std::move(done)) {}
  ~DisplayBarrier() override {
    if (callback_) {
      wl_callback_destroy(callback_);
    }
    if (queue_) {
      wl_event_queue_destroy(queue_);
    }
  }

  bool start() {
    auto* wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    display_ = wayland ? wayland->display() : nullptr;
    if (!display_) {
      return false;
    }
    queue_ = wl_display_create_queue(display_);
    if (!queue_) {
      return false;
    }
    auto* wrapper = wl_proxy_create_wrapper(display_);
    if (!wrapper) {
      return false;
    }
    wl_proxy_set_queue(static_cast<wl_proxy*>(wrapper), queue_);
    callback_ = wl_display_sync(static_cast<wl_display*>(wrapper));
    wl_proxy_wrapper_destroy(wrapper);
    if (!callback_) {
      return false;
    }
    static constexpr wl_callback_listener listener{.done = [](void* data, wl_callback*, uint32_t) {
      auto* self = static_cast<DisplayBarrier*>(data);
      self->complete(true);
    }};
    wl_callback_add_listener(callback_, &listener, this);
    wl_display_flush(display_);
    connect(&poll_, &QTimer::timeout, this, [this] {
      if (wl_display_dispatch_queue_pending(display_, queue_) < 0) {
        complete(false);
      }
    });
    QTimer::singleShot(2000, this, [this] { complete(false); });
    poll_.start(10);
    return true;
  }

 private:
  void complete(bool ok) {
    if (done_) {
      poll_.stop();
      auto done = std::exchange(done_, {});
      done(ok);
    }
  }

  std::function<void(bool)> done_;
  QTimer poll_;
  wl_display* display_ = nullptr;
  wl_event_queue* queue_ = nullptr;
  wl_callback* callback_ = nullptr;
};

}  // namespace

class ScreenshotRequest final : public QDBusVirtualObject {
  Q_OBJECT

 public:
  ScreenshotRequest(QDBusConnection connection, QDBusMessage call, QString handle, CaptureOptions options,
                    std::function<void()> retired, ScreenshotPortal::Approval approval,
                    ScreenshotPortal::Capture capture)
      : connection_(std::move(connection)),
        call_(std::move(call)),
        handle_(std::move(handle)),
        options_(options),
        retired_(std::move(retired)),
        approval_(std::move(approval)),
        capture_(std::move(capture)),
        broker_(Broker, connection_, QDBusServiceWatcher::WatchForOwnerChange) {
    connect(&broker_, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString&, const QString& oldOwner, const QString& newOwner) {
              if (oldOwner == call_.service() && newOwner != oldOwner) {
                finish(1);
              }
            });
  }

  QString introspect(const QString&) const override {
    return R"(<interface name="org.freedesktop.impl.portal.Request"><method name="Close"/></interface>)";
  }

  bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override {
    if (message.interface() != RequestInterface || message.member() != "Close") {
      return false;
    }
    if (message.service() != call_.service() || !isBroker(connection, message.service())) {
      return connection.send(
          message.createErrorReply(QDBusError::AccessDenied, "Only the requesting broker may close."));
    }
    if (!message.signature().isEmpty()) {
      return connection.send(message.createErrorReply(QDBusError::InvalidArgs, "Close takes no arguments."));
    }
    connection.send(message.createReply());
    finish(1);
    return true;
  }

  void start() {
    if (options_.approved) {
      acquire();
      return;
    }
    if (approval_) {
      approval_(options_.target, [request = QPointer(this)](bool area) {
        if (request) {
          request->approve(area);
        }
      });
      return;
    }
    if (!qGuiApp || !qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
      finish(2);
      return;
    }

    static const bool resourceReady = [] {
      initializeApprovalResource();
      return true;
    }();
    Q_UNUSED(resourceReady);
    dialog_ = std::make_unique<QQuickView>();
    dialog_->setTitle(tr("Screen Capture Request"));
    dialog_->setResizeMode(QQuickView::SizeViewToRootObject);
    dialog_->setModality(options_.modal ? Qt::WindowModal : Qt::NonModal);
    dialog_->setInitialProperties({
        {"request", QVariant::fromValue(static_cast<QObject*>(this))},
        {"appName", call_.arguments()[1].toString()},
        {"choice", options_.target == Target::Choice},
        {"description", options_.target == Target::Color    ? tr("pick a screen color")
                        : options_.target == Target::Area   ? tr("capture a screen area")
                        : options_.target == Target::Choice ? tr("take a screenshot")
                                                            : tr("capture your desktop")},
    });
    dialog_->setSource(QUrl("qrc:/screenshot-portal/Approval.qml"));
    if (dialog_->status() != QQuickView::Ready) {
      finish(2);
      return;
    }
    connect(dialog_.get(), &QWindow::visibleChanged, this, [this](bool visible) {
      if (!visible && !approved_) {
        reject();
      }
    });
    auto* parent = new WaylandParent(dialog_.get(), call_.arguments()[2].toString(), dialog_.get());
    connect(parent, &WaylandParent::prepared, this, [this] {
      if (!finished_ && !approved_) {
        dialog_->show();
        dialog_->requestActivate();
      }
    });
    parent->prepare();
  }

  Q_INVOKABLE void approve(bool area) {
    if (finished_ || approved_ || rejected_ || (!approval_ && (!dialog_ || !dialog_->isVisible()))) {
      return;
    }
    approved_ = true;
    if (options_.target == Target::Choice) {
      options_.target = area ? Target::Area : Target::Desktop;
    }
    if (approval_) {
      acquire();
      return;
    }
    // Return from the QML handler before destroying its window.
    QTimer::singleShot(0, this, [this] {
      if (finished_ || rejected_) {
        return;
      }
      dialog_.reset();
      auto* barrier = new DisplayBarrier(this, [this](bool ok) {
        if (!finished_ && !rejected_) {
          if (ok) {
            acquire();
          } else {
            finish(2);
          }
        }
      });
      if (!barrier->start()) {
        finish(2);
      }
    });
  }

  Q_INVOKABLE void reject() {
    rejected_ = true;
    QTimer::singleShot(0, this, [this] { finish(1); });
  }

  bool finish(uint response, const QVariantMap& results = {}) {
    if (std::exchange(finished_, true)) {
      return false;
    }
    if (dialog_) {
      dialog_->hide();
    }
    if (controller_) {
      controller_->cancel();
    }
    if (overlay_) {
      overlay_->cancel();
    }
    if (acquisition_) {
      acquisition_->cancel();
    }
    connection_.unregisterObject(handle_);
    if (response == 0 && !isBroker(connection_, call_.service())) {
      response = 1;
    }
    const bool delivered = reply(connection_, call_, response, response == 0 ? results : QVariantMap{});
    retired_();
    deleteLater();
    return delivered && response == 0;
  }

 private:
  void acquire() {
    if (!isBroker(connection_, call_.service())) {
      finish(1);
      return;
    }
    if (capture_) {
      capture_(
          options_.target,
          [request = QPointer(this)](const QImage& image) {
            if (request && !request->finished_) {
              request->publish(image);
            }
          },
          [request = QPointer(this)](const QColor& color) {
            if (request && !request->finished_) {
              request->publishColor(color);
            }
          });
      return;
    }
    if (!qGuiApp || !qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
      finish(2);
      return;
    }
    const auto layoutChanged = [this] { finish(2); };
    connect(qGuiApp, &QGuiApplication::screenAdded, this, layoutChanged);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, layoutChanged);
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, layoutChanged);
    for (auto* screen : QGuiApplication::screens()) {
      connect(screen, &QScreen::geometryChanged, this, layoutChanged);
      connect(screen, &QScreen::orientationChanged, this, layoutChanged);
    }
    acquisition_ = std::make_unique<screenshot::GrimAcquisition>();
    controller_ = std::make_unique<screenshot::CaptureController>(*acquisition_, std::chrono::milliseconds(0),
                                                                  screenshot::CursorPolicy::Exclude);
    connect(controller_.get(), &screenshot::CaptureController::failed, this, [this] { finish(2); });
    connect(controller_.get(), &screenshot::CaptureController::captured, this, [this](const auto& frame) {
      if (finished_) {
        return;
      }
      if (options_.target == Target::Desktop) {
        publish(frame.image());
        return;
      }
      const auto mode = options_.target == Target::Color ? screenshot::RegionOverlay::Mode::PickColor
                                                         : screenshot::RegionOverlay::Mode::Share;
      overlay_ = std::make_unique<screenshot::RegionOverlay>(frame, *acquisition_, mode);
      connect(overlay_.get(), &screenshot::RegionOverlay::canceled, this, [this] { finish(1); });
      connect(overlay_.get(), &screenshot::RegionOverlay::failed, this, [this] { finish(2); });
      connect(overlay_.get(), &screenshot::RegionOverlay::selected, this,
              [this](auto, const QImage& image) { publish(image); });
      connect(overlay_.get(), &screenshot::RegionOverlay::colorPicked, this, &ScreenshotRequest::publishColor);
      overlay_->open();
    });
  }

  void publishColor(const QColor& color) {
    finish(0, {{"color", QVariant::fromValue(SettingsPortalAccentColor{std::clamp<double>(color.redF(), 0, 1),
                                                                       std::clamp<double>(color.greenF(), 0, 1),
                                                                       std::clamp<double>(color.blueF(), 0, 1)})}});
  }

  void publish(const QImage& image) {
    const auto runtimePath = qgetenv("XDG_RUNTIME_DIR");
    const auto runtime = QFile::decodeName(runtimePath);
    if (!QDir::isAbsolutePath(runtime) || QFile::encodeName(runtime) != runtimePath || !QDir(runtime).exists()) {
      finish(2);
      return;
    }
    auto directory = std::make_shared<QTemporaryDir>(QDir(runtime).filePath("holonight-screenshot-XXXXXX"));
    if (!directory->isValid()) {
      finish(2);
      return;
    }
    const auto png = directory->filePath("Screenshot.png");
    // The worker keeps the directory alive if the request closes during encoding.
    auto* encoding = new QFutureWatcher<bool>(this);
    connect(encoding, &QFutureWatcher<bool>::finished, this, [this, encoding, directory, png] {
      if (finished_) {
        return;
      }
      if (!encoding->result()) {
        finish(2);
        return;
      }
      if (finish(0, {{"uri", localUri(QFile::encodeName(png))}})) {
        directory->setAutoRemove(false);
      }
    });
    encoding->setFuture(QtConcurrent::run(
        [directory, image] { return screenshot::savePng(directory->filePath("Screenshot.png"), image).isEmpty(); }));
  }

  QDBusConnection connection_;
  QDBusMessage call_;
  QString handle_;
  CaptureOptions options_;
  std::function<void()> retired_;
  ScreenshotPortal::Approval approval_;
  ScreenshotPortal::Capture capture_;
  QDBusServiceWatcher broker_;
  std::unique_ptr<QQuickView> dialog_;
  std::unique_ptr<screenshot::GrimAcquisition> acquisition_;
  std::unique_ptr<screenshot::CaptureController> controller_;
  std::unique_ptr<screenshot::RegionOverlay> overlay_;
  bool approved_ = false;
  bool rejected_ = false;
  bool finished_ = false;
};

ScreenshotPortal::ScreenshotPortal(Approval approval, Capture capture)
    : approval_(std::move(approval)), capture_(std::move(capture)) {}

ScreenshotPortal::~ScreenshotPortal() { shutdown(); }

QString ScreenshotPortal::introspection() {
  QString xml = R"(<interface name="org.freedesktop.impl.portal.Screenshot">
    <property name="version" type="u" access="read"/>
    <property name="AvailableTargets" type="u" access="read"/>)";
  for (const auto* method : {"Screenshot", "PickColor"}) {
    xml += QString(R"(<method name="%1">
      <arg name="handle" type="o" direction="in"/>
      <arg name="app_id" type="s" direction="in"/>
      <arg name="parent_window" type="s" direction="in"/>
      <arg name="options" type="a{sv}" direction="in"/>
      <arg name="response" type="u" direction="out"/>
      <arg name="results" type="a{sv}" direction="out"/>
      </method>)")
               .arg(method);
  }
  return xml + "</interface>";
}

bool ScreenshotPortal::handleMessage(const QDBusMessage& message, QDBusConnection connection, bool stopping) {
  const auto args = message.arguments();
  if (message.interface() == "org.freedesktop.DBus.Properties" && !args.isEmpty() && args[0].toString() == Interface) {
    const QVariantMap properties{{"version", 3U}, {"AvailableTargets", 5U}};
    if (message.member() == "GetAll" && message.signature() == "s") {
      return connection.send(message.createReply(QVariant::fromValue(properties)));
    }
    if (message.member() == "Get" && message.signature() == "ss") {
      const auto value = properties.value(args[1].toString());
      if (value.isValid()) {
        return connection.send(message.createReply({QVariant::fromValue(QDBusVariant(value))}));
      }
      return connection.send(message.createErrorReply(QDBusError::UnknownProperty, "Unknown Screenshot property."));
    }
    if (message.member() == "Set" && message.signature() == "ssv") {
      return connection.send(
          message.createErrorReply(QDBusError::PropertyReadOnly, "Screenshot properties are read-only."));
    }
    return connection.send(message.createErrorReply(QDBusError::InvalidArgs, "Invalid Screenshot property arguments."));
  }
  const bool pickColor = message.member() == "PickColor";
  if (message.interface() != Interface || (!pickColor && message.member() != "Screenshot")) {
    return false;
  }
  if (!isBroker(connection, message.service())) {
    return connection.send(
        message.createErrorReply(QDBusError::AccessDenied, "Only the current portal broker may capture."));
  }
  if (message.signature() != "ossa{sv}") {
    return connection.send(message.createErrorReply(QDBusError::InvalidArgs, "Expected ossa{sv}."));
  }
  const auto handle = args[0].value<QDBusObjectPath>().path();
  const auto options = captureOptions(pickColor, decode<QVariantMap>(args[3]));
  if (!options || stopping || active_ || handle == DesktopPath || handle == "/") {
    return reply(connection, message, 2);
  }
  auto* request =
      new ScreenshotRequest(connection, message, handle, *options, [this] { active_.clear(); }, approval_, capture_);
  if (!connection.registerVirtualObject(handle, request)) {
    delete request;
    return reply(connection, message, 2);
  }
  message.setDelayedReply(true);
  request->setParent(this);
  active_ = request;
  request->start();
  return true;
}

void ScreenshotPortal::shutdown() {
  if (active_) {
    active_->finish(2);
  }
}

}  // namespace Portal

#include "screenshot.moc"
