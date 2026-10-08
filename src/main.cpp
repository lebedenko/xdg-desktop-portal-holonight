#include "backend.h"
#include "presenter.h"

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QSocketNotifier>

#include <array>
#include <cerrno>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

namespace {
// The signal handler can only write to this self-pipe.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
volatile std::sig_atomic_t signalWrite = -1;
void terminate(int /*signal*/) {
  const int savedErrno = errno;
  const char byte = 1;
  const auto ignored = ::write(signalWrite, &byte, 1);
  (void)ignored;
  errno = savedErrno;
}
}  // namespace
int main(int argc, char** argv) {
  QGuiApplication app(argc, argv);
  QCoreApplication::setApplicationName("xdg-desktop-portal-holonight");
  QCoreApplication::setApplicationVersion("0.1.0");
  QGuiApplication::setQuitOnLastWindowClosed(false);
  if (!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE")) {
    QQuickStyle::setStyle("Holonight");
  }
  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();
  parser.process(app);
  Portal::QmlPresenter presenter;
  Portal::Backend backend(QDBusConnection::sessionBus(), &presenter);
  QObject::connect(&backend, &Portal::Backend::disconnected, &app, &QCoreApplication::quit);
  if (!backend.start()) {
    return 1;
  }
  std::array<int, 2> signalSockets{};
  if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0, signalSockets.data()) != 0) {
    return 1;
  }
  signalWrite = signalSockets[1];
  QSocketNotifier notifier(signalSockets[0], QSocketNotifier::Read);
  QObject::connect(&notifier, &QSocketNotifier::activated, &app, [&] {
    backend.shutdown();
    QGuiApplication::quit();
  });
  std::signal(SIGTERM, terminate);
  std::signal(SIGINT, terminate);
  const auto result = QGuiApplication::exec();
  signalWrite = -1;
  ::close(signalSockets[0]);
  ::close(signalSockets[1]);
  return result;
}
