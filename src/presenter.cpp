#include "presenter.h"

#include "wayland_parent.h"

#include <QDebug>
#include <QQmlComponent>

#include <HolonightFileBrowser/quick.h>

// Qt generates this external symbol from the module URI.
// NOLINTNEXTLINE(readability-identifier-naming)
void qml_register_types_HolonightPortal_Private();

namespace Portal {
QmlPresenter::QmlPresenter(QObject* parent) : Presenter(parent) {
  static const bool registered = [] {
    qml_register_types_HolonightPortal_Private();
    return true;
  }();
  (void)registered;
  HolonightFileBrowser::initializeEngine(engine_);
}
QmlPresenter::~QmlPresenter() { qDeleteAll(windows_); }
bool QmlPresenter::show(Controller* controller, const Options& options) {
  QQmlComponent component(&engine_, QUrl("qrc:/qt/qml/HolonightPortal/Chooser.qml"));
  auto* object = component.createWithInitialProperties({{"controller", QVariant::fromValue(controller)}});
  auto* window = qobject_cast<QWindow*>(object);
  if (window == nullptr) {
    qWarning() << component.errors();
    delete object;
    return false;
  }
  window->setModality(options.modal ? Qt::WindowModal : Qt::NonModal);
  windows_.insert(controller, window);
  window->create();
  new WaylandParent(window, options.parent, window);
  window->show();
  return true;
}
void QmlPresenter::close(Controller* controller) {
  if (auto* window = windows_.take(controller)) {
    window->hide();
    window->deleteLater();
  }
}
}  // namespace Portal
