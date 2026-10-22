#pragma once

#include "backend.h"

class DemoController : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
  Q_PROPERTY(QString log READ log NOTIFY logChanged)
 public:
  explicit DemoController(QDBusConnection connection, QObject* parent = nullptr);
  [[nodiscard]] bool enabled() const { return ready_ && !busy_; }
  [[nodiscard]] QString log() const { return log_; }
  void ready(const QString& handle);
  void fail(const QString& detail);
  Q_INVOKABLE void request(int picker);
 signals:
  void stateChanged();
  void logChanged();

 private:
  void append(const QString& text);
  QDBusConnection connection_;
  QString handle_;
  QString log_;
  quint64 serial_ = 0;
  bool ready_ = false;
  bool busy_ = false;
};
