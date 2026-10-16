#include "filesystem.h"

#include <QSet>

#include <HolonightFileBrowser/native_path.h>
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>

namespace Portal {
QByteArray unterminate(QByteArray value) {
  if (value.endsWith('\0')) {
    value.chop(1);
  }
  return value;
}
bool validName(const QByteArray& name) {
  return !name.isEmpty() && name != "." && name != ".." && !name.contains('/') && !name.contains('\0');
}
bool absolutePath(const QByteArray& path) { return path.startsWith('/') && !path.contains('\0'); }
bool directoryExists(const QByteArray& path) {
  struct stat metadata{};
  return absolutePath(path) && ::stat(path.constData(), &metadata) == 0 && S_ISDIR(metadata.st_mode);
}
QByteArray parentPath(const QByteArray& value) {
  QByteArray path = value;
  while (path.size() > 1 && path.endsWith('/')) {
    path.chop(1);
  }
  const auto slash = path.lastIndexOf('/');
  return slash <= 0 ? QByteArray("/") : path.left(slash);
}
QByteArray joinPath(const QByteArray& folder, const QByteArray& name) {
  return folder + (folder.endsWith('/') ? "" : "/") + name;
}
QString localUri(const QByteArray& path) { return HolonightFileBrowser::localUrl(path).toString(QUrl::FullyEncoded); }
namespace {
Validation validateOpen(bool directory, const QList<QByteArray>& paths,
                        const std::shared_ptr<std::atomic_bool>& cancelled) {
  for (const auto& path : paths) {
    if (cancelled->load()) {
      return {};
    }
    struct stat metadata{};
    if (::stat(path.constData(), &metadata) != 0 || directory != S_ISDIR(metadata.st_mode)) {
      return {.paths = {}, .error = "A selected entry is unavailable or has changed type.", .overwrite = false};
    }
  }
  return {.paths = paths, .error = {}, .overwrite = false};
}
Validation validateSave(const QByteArray& path) {
  const auto name = path.mid(path.lastIndexOf('/') + 1);
  if (!validName(name)) {
    return {.paths = {}, .error = "Enter a valid filename.", .overwrite = false};
  }
  struct stat metadata{};
  const bool exists = ::lstat(path.constData(), &metadata) == 0;
  if (!exists && errno != ENOENT) {
    return {.paths = {}, .error = "Cannot inspect the destination.", .overwrite = false};
  }
  struct stat target{};
  if (exists &&
      (::stat(path.constData(), &target) != 0 || S_ISDIR(target.st_mode) || ::access(path.constData(), W_OK) != 0)) {
    return {.paths = {}, .error = "The existing destination is not a writable file.", .overwrite = false};
  }
  return {.paths = {path}, .error = {}, .overwrite = exists};
}
QByteArray numberedName(const QByteArray& name, int number) {
  auto dot = name.lastIndexOf('.');
  if (dot <= 0) {
    dot = name.size();
  }
  return name.left(dot) + " (" + QByteArray::number(number) + ")" + name.mid(dot);
}
Validation validateMany(const QByteArray& folder, const Names& names,
                        const std::shared_ptr<std::atomic_bool>& cancelled) {
  if (names.isEmpty()) {
    return {.paths = {}, .error = "No filenames were supplied.", .overwrite = false};
  }
  Validation result;
  QSet<QByteArray> used;
  for (const auto& supplied : names) {
    const auto name = unterminate(supplied);
    if (!validName(name)) {
      return {.paths = {}, .error = "A filename is empty or contains an invalid path component.", .overwrite = false};
    }
    int number = 0;
    for (;;) {
      if (cancelled->load()) {
        return {};
      }
      const auto path = joinPath(folder, number == 0 ? name : numberedName(name, number));
      struct stat metadata{};
      const bool exists = ::lstat(path.constData(), &metadata) == 0;
      if (!exists && errno != ENOENT) {
        return {.paths = {}, .error = "Cannot inspect the destination.", .overwrite = false};
      }
      if (!exists && !used.contains(path)) {
        used.insert(path);
        result.paths.append(path);
        break;
      }
      ++number;
    }
  }
  return result;
}
}  // namespace
Validation validate(Operation operation, bool directory, const QList<QByteArray>& paths, const Names& names,
                    const std::shared_ptr<std::atomic_bool>& cancelled) {
  if (paths.isEmpty()) {
    return {.paths = {}, .error = directory ? "Select a folder." : "Select a file.", .overwrite = false};
  }
  for (const auto& path : paths) {
    if (!absolutePath(path)) {
      return {.paths = {}, .error = "Select an absolute local path.", .overwrite = false};
    }
  }
  if (operation == Operation::Open) {
    return validateOpen(directory, paths, cancelled);
  }
  const auto folder = operation == Operation::Save ? parentPath(paths.front()) : paths.front();
  if (!directoryExists(folder) || ::access(folder.constData(), W_OK | X_OK) != 0) {
    return {.paths = {}, .error = "The destination folder is unavailable or not writable.", .overwrite = false};
  }
  if (cancelled->load()) {
    return {};
  }
  return operation == Operation::Save ? validateSave(paths.front()) : validateMany(folder, names, cancelled);
}
}  // namespace Portal
