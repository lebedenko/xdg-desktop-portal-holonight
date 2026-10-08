#pragma once
#include <QObject>

#include <memory>
class QWindow;
namespace Portal {
// Private native integration; no GuiPrivate types escape this header.
class WaylandParent : public QObject {
 public:
  WaylandParent(QWindow* window, const QString& parent, QObject* owner);
  ~WaylandParent() override;
  WaylandParent(const WaylandParent&) = delete;
  WaylandParent& operator=(const WaylandParent&) = delete;
  WaylandParent(WaylandParent&&) = delete;
  WaylandParent& operator=(WaylandParent&&) = delete;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};
}  // namespace Portal
