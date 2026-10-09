#pragma once
#include "backend.h"

#include <QQmlEngine>
#include <QWindow>

#include <functional>

namespace Portal {
class WaylandParent;
class QmlPresenter : public Presenter {
 public:
  explicit QmlPresenter(QObject* parent = nullptr);
  using ParentFactory = std::function<WaylandParent*(QWindow*, const QString&)>;
  explicit QmlPresenter(ParentFactory factory, QObject* parent = nullptr);
  ~QmlPresenter() override;
  QmlPresenter(const QmlPresenter&) = delete;
  QmlPresenter& operator=(const QmlPresenter&) = delete;
  QmlPresenter(QmlPresenter&&) = delete;
  QmlPresenter& operator=(QmlPresenter&&) = delete;
  bool show(Controller* controller, const Options& options) override;
  void close(Controller* controller) override;

 private:
  ParentFactory parent_factory_;
  QQmlEngine engine_;
  QHash<Controller*, QWindow*> windows_;
};
}  // namespace Portal
