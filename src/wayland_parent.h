#pragma once
#include <QObject>
#include <QTimer>

#include <memory>
class QWindow;
namespace Portal {
// Private native integration; no GuiPrivate types escape this header.
class WaylandParent : public QObject {
  Q_OBJECT
 public:
  WaylandParent(QWindow* window, const QString& parent, QObject* owner);
  ~WaylandParent() override;
  void prepare();

 signals:
  void prepared();

 protected:
  explicit WaylandParent(QObject* owner);
  virtual void prepareNative();
  void complete(bool parent_ready = false);
  template <typename Role>
  void watchRole(Role* role) {
    connect(role, &Role::surfaceRoleCreated, this, &WaylandParent::onSurfaceRoleCreated, Qt::DirectConnection);
  }
  virtual void applyParent();

 public:
  WaylandParent(const WaylandParent&) = delete;
  WaylandParent& operator=(const WaylandParent&) = delete;
  WaylandParent(WaylandParent&&) = delete;
  WaylandParent& operator=(WaylandParent&&) = delete;

 private:
  void onSurfaceRoleCreated();
  struct Impl;
  std::unique_ptr<Impl> impl_;
  QTimer deadline_;
  bool started_ = false;
  bool completed_ = false;
  bool parent_ready_ = false;
};
}  // namespace Portal
