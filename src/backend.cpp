#include "backend.h"

#include <QDBusObjectPath>

namespace Portal {
Request::Request(Backend* backend, QDBusMessage call, Options options)
    : QDBusVirtualObject(backend),
      backend_(backend),
      call_(std::move(call)),
      controller_(std::move(options)),
      owner_(call_.service(), backend->connection(), QDBusServiceWatcher::WatchForUnregistration) {
  connect(&controller_, &Controller::completed, this, &Request::complete);
  connect(&owner_, &QDBusServiceWatcher::serviceUnregistered, this, [this] { complete(1, {}); });
}
QString Request::introspect(const QString& /*path*/) const {
  return R"(<interface name="org.freedesktop.impl.portal.Request"><method name="Close"/></interface>)";
}
bool Request::handleMessage(const QDBusMessage& message, const QDBusConnection& connection) {
  if (message.interface() != "org.freedesktop.impl.portal.Request" || message.member() != "Close") {
    return false;
  }
  if (message.service() != call_.service()) {
    connection.send(
        message.createErrorReply(QDBusError::AccessDenied, "Only the requesting broker may close this request."));
    return true;
  }
  if (!message.arguments().isEmpty()) {
    connection.send(message.createErrorReply(QDBusError::InvalidArgs, "Close takes no arguments."));
    return true;
  }
  connection.send(message.createReply());
  complete(1, {});
  return true;
}
void Request::complete(uint response, const QVariantMap& results) {
  if (completed_) {
    return;
  }
  completed_ = true;
  controller_.stop();
  backend_->retire(this, response, results);
}
Backend::Backend(QDBusConnection connection, Presenter* presenter, QObject* parent)
    : QDBusVirtualObject(parent), connection_(std::move(connection)), presenter_(presenter) {
  registerWireTypes();
  connection_.connect({}, "/org/freedesktop/DBus/Local", "org.freedesktop.DBus.Local", "Disconnected", this,
                      SLOT(connectionLost()));
}
void Backend::connectionLost() {
  shutdown();
  emit disconnected();
}
Backend::~Backend() {
  shutdown();
  connection_.unregisterObject(DesktopPath);
  connection_.unregisterService(BusName);
}
bool Backend::start() {
  return connection_.registerVirtualObject(DesktopPath, this) && connection_.registerService(BusName);
}
void Backend::shutdown() {
  stopping_ = true;
  const auto requests = requests_.values();
  for (auto* request : requests) {
    request->complete(2, {});
  }
}
QString Backend::introspect(const QString& /*path*/) const {
  QString xml = "<interface name=\"org.freedesktop.impl.portal.FileChooser\">";
  for (const auto* method : {"OpenFile", "SaveFile", "SaveFiles"}) {
    xml +=
        QString(
            "<method name=\"%1\"><arg name=\"handle\" type=\"o\" direction=\"in\"/>"
            "<arg name=\"app_id\" type=\"s\" direction=\"in\"/><arg name=\"parent_window\" type=\"s\" "
            "direction=\"in\"/>"
            "<arg name=\"title\" type=\"s\" direction=\"in\"/><arg name=\"options\" type=\"a{sv}\" direction=\"in\"/>"
            "<arg name=\"response\" type=\"u\" direction=\"out\"/><arg name=\"results\" type=\"a{sv}\" "
            "direction=\"out\"/>"
            "</method>")
            .arg(method);
  }
  return xml + "</interface>";
}
bool Backend::handleMessage(const QDBusMessage& message, const QDBusConnection& connection) {
  if (message.interface() != "org.freedesktop.impl.portal.FileChooser") {
    return false;
  }
  Operation operation = Operation::Open;
  if (message.member() == "OpenFile") {
    operation = Operation::Open;
  } else if (message.member() == "SaveFile") {
    operation = Operation::Save;
  } else if (message.member() == "SaveFiles") {
    operation = Operation::SaveMany;
  } else {
    return false;
  }
  if (message.signature() != "osssa{sv}") {
    connection.send(message.createErrorReply(QDBusError::InvalidArgs, "Expected osssa{sv}."));
    return true;
  }
  const auto args = message.arguments();
  const auto handle = args[0].value<QDBusObjectPath>().path();
  if (stopping_ || requests_.contains(handle) || handle == DesktopPath || handle == "/") {
    connection.send(message.createReply({QVariant::fromValue(2U), QVariantMap{}}));
    return true;
  }
  const auto options = decodeOptions(operation, args[3].toString(), args[2].toString(), decode<QVariantMap>(args[4]));
  message.setDelayedReply(true);
  auto* request = new Request(this, message, options);
  if (!connection_.registerVirtualObject(handle, request)) {
    connection.send(message.createReply({QVariant::fromValue(2U), QVariantMap{}}));
    request->deleteLater();
    return true;
  }
  requests_.insert(handle, request);
  if (!presenter_->show(request->controller(), options)) {
    request->complete(2, {});
  }
  return true;
}
void Backend::retire(Request* request, uint response, const QVariantMap& results) {
  presenter_->close(request->controller());
  const auto handle = request->call().arguments()[0].value<QDBusObjectPath>().path();
  connection_.unregisterObject(handle);
  requests_.remove(handle);
  connection_.send(request->call().createReply({QVariant::fromValue(response), results}));
  request->deleteLater();
}
}  // namespace Portal
