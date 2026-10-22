#include "demo_controller.h"

#include <QDBusObjectPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDateTime>
#include <QDir>

#include <limits>

namespace {
const QStringList Labels{"Open file", "Open multiple files", "Select folder", "Select multiple folders",
                         "Save file", "Save multiple files"};
}
DemoController::DemoController(QDBusConnection connection, QObject* parent)
    : QObject(parent), connection_(std::move(connection)) {
  Portal::registerWireTypes();
  append("Starting local backend and preparing parent window…");
}
void DemoController::append(const QString& text) {
  log_ += QDateTime::currentDateTime().toString("[HH:mm:ss] ") + text + '\n';
  emit logChanged();
}
void DemoController::ready(const QString& handle) {
  handle_ = handle;
  if (!ready_) {
    ready_ = true;
    append(handle.isEmpty() ? "Ready (unparented)." : "Ready (Wayland parent exported).");
    emit stateChanged();
  }
}
void DemoController::fail(const QString& detail) {
  ready_ = false;
  append("Failed: " + detail);
  emit stateChanged();
}
void DemoController::request(int picker) {
  if (!enabled() || picker < 0 || picker >= Labels.size()) {
    return;
  }
  busy_ = true;
  emit stateChanged();
  const auto& label = Labels.at(picker);
  const auto path = QString("/demo/request/r%1").arg(++serial_);
  QVariantMap options{{"current_folder", QFile::encodeName(QDir::homePath()) + '\0'}, {"modal", true}};
  if (picker < 4) {
    options.insert("multiple", picker == 1 || picker == 3);
    options.insert("directory", picker >= 2);
  } else if (picker == 4) {
    options.insert("current_name", "portal-demo.txt");
  } else {
    options.insert("files", QVariant::fromValue(Portal::Names{"portal-demo-one.txt", "portal-demo-two.txt"}));
  }
  const char* method = "OpenFile";
  if (picker == 4) {
    method = "SaveFile";
  } else if (picker == 5) {
    method = "SaveFiles";
  }
  auto message = QDBusMessage::createMethodCall(Portal::BusName, Portal::DesktopPath,
                                                "org.freedesktop.impl.portal.FileChooser", method);
  message.setArguments({QVariant::fromValue(QDBusObjectPath(path)), "native.demo", handle_, label, options});
  append(label + " requested (" + path + ").");
  auto* watcher = new QDBusPendingCallWatcher(connection_.asyncCall(message, std::numeric_limits<int>::max()), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, label] {
    QDBusPendingReply<uint, QVariantMap> reply(*watcher);
    if (reply.isError()) {
      append(label + " failed: " + reply.error().name() + ": " + reply.error().message());
    } else {
      const uint code = reply.argumentAt<0>();
      QString status = "failed";
      if (code == 0) {
        status = "accepted";
      } else if (code == 1) {
        status = "cancelled";
      }
      append(label + " " + status + QString(" (response %1).").arg(code));
      for (const auto& uri : reply.argumentAt<1>().value("uris").toStringList()) {
        append("  " + uri);
      }
    }
    busy_ = false;
    emit stateChanged();
    watcher->deleteLater();
  });
}
