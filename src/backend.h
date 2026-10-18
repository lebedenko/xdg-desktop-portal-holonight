#pragma once
#include "controller.h"
#include "settings.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusServiceWatcher>
#include <QDBusVirtualObject>

#include <holonight/appearance_reader.h>

namespace Portal {
inline constexpr auto BusName = "org.freedesktop.impl.portal.desktop.holonight";
inline constexpr auto DesktopPath = "/org/freedesktop/portal/desktop";
class Presenter : public QObject {
  Q_OBJECT
 public:
  using QObject::QObject;
  virtual bool show(Controller* controller, const Options& options) = 0;
  virtual void close(Controller* controller) = 0;
};
class Backend;
class Request : public QDBusVirtualObject {
  Q_OBJECT
 public:
  Request(Backend* backend, QDBusMessage call, Options options);
  [[nodiscard]] QString introspect(const QString& path) const override;
  bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override;
  Controller* controller() { return &controller_; }
  [[nodiscard]] const QDBusMessage& call() const { return call_; }
  void complete(uint response, const QVariantMap& results);

 private:
  Backend* backend_;
  QDBusMessage call_;
  Controller controller_;
  QDBusServiceWatcher owner_;
  bool completed_ = false;
};
class Backend : public QDBusVirtualObject {
  Q_OBJECT
 public:
  Backend(QDBusConnection connection, Presenter* presenter, QObject* parent = nullptr);
  ~Backend() override;
  Backend(const Backend&) = delete;
  Backend& operator=(const Backend&) = delete;
  Backend(Backend&&) = delete;
  Backend& operator=(Backend&&) = delete;
  bool start();
  void shutdown();
  [[nodiscard]] QString introspect(const QString& path) const override;
  bool handleMessage(const QDBusMessage& message, const QDBusConnection& connection) override;
  void retire(Request* request, uint response, const QVariantMap& results);
  [[nodiscard]] QDBusConnection connection() const { return connection_; }
  [[nodiscard]] qsizetype activeRequests() const { return requests_.size(); }
 signals:
  void disconnected();
 private slots:
  void connectionLost();

 private:
  QDBusConnection connection_;
  Presenter* presenter_;
  QHash<QString, Request*> requests_;
  Holonight::AppearanceReader appearance_;
  SettingsPortalBackend settings_;
  bool object_registered_ = false;
  bool service_registered_ = false;
  bool stopping_ = false;
};
}  // namespace Portal
