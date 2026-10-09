#include "presenter.h"

#include "wayland_parent.h"

#include <QDebug>
#include <QQmlComponent>

#include <HolonightFileBrowser/quick.h>

// Qt generates this external symbol from the module URI.
// NOLINTNEXTLINE(readability-identifier-naming)
void qml_register_types_HolonightPortal_Private();

namespace Portal {
QmlPresenter::QmlPresenter(QObject* parent)
    : QmlPresenter([](QWindow* window, const QString& handle) { return new WaylandParent(window, handle, window); },
                   parent) {}
QmlPresenter::QmlPresenter(ParentFactory factory, QObject* parent)
    : Presenter(parent), parent_factory_(std::move(factory)) {
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
  auto* adapter = parent_factory_(window, options.parent);
  connect(adapter, &WaylandParent::prepared, window, &QWindow::show);
  adapter->prepare();
  return true;
}
void QmlPresenter::close(Controller* controller) {
  if (auto* window = windows_.take(controller)) {
    qDeleteAll(window->findChildren<WaylandParent*>(QString{}, Qt::FindDirectChildrenOnly));
    window->hide();
    window->deleteLater();
  }
}
}  // namespace Portal
