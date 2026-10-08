#pragma once
#include "backend.h"

#include <QQmlEngine>
#include <QWindow>

namespace Portal {
class QmlPresenter : public Presenter {
 public:
  explicit QmlPresenter(QObject* parent = nullptr);
  ~QmlPresenter() override;
  QmlPresenter(const QmlPresenter&) = delete;
  QmlPresenter& operator=(const QmlPresenter&) = delete;
  QmlPresenter(QmlPresenter&&) = delete;
  QmlPresenter& operator=(QmlPresenter&&) = delete;
  bool show(Controller* controller, const Options& options) override;
  void close(Controller* controller) override;

 private:
  QQmlEngine engine_;
  QHash<Controller*, QWindow*> windows_;
};
}  // namespace Portal
