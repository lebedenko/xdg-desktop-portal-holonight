#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QPointer>

#include <functional>

class QColor;
class QImage;

namespace Portal {
class ScreenshotRequest;

class ScreenshotPortal final : public QObject {
 public:
  enum class Target { Desktop, Area, Choice, Color };
  using Approval = std::function<void(Target, std::function<void(bool)>)>;
  using Capture = std::function<void(Target, std::function<void(const QImage&)>, std::function<void(const QColor&)>)>;

  explicit ScreenshotPortal(Approval approval = {}, Capture capture = {});
  ~ScreenshotPortal() override;
  static QString introspection();
  bool handleMessage(const QDBusMessage& message, QDBusConnection connection, bool stopping);
  void shutdown();

 private:
  Approval approval_;
  Capture capture_;
  QPointer<ScreenshotRequest> active_;
};
}  // namespace Portal
