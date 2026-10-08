#pragma once
#include "wire.h"

#include <atomic>
#include <memory>

namespace Portal {
QByteArray unterminate(QByteArray value);
bool validName(const QByteArray& name);
bool absolutePath(const QByteArray& path);
bool directoryExists(const QByteArray& path);
QByteArray parentPath(const QByteArray& value);
QByteArray joinPath(const QByteArray& folder, const QByteArray& name);
QString localUri(const QByteArray& path);
struct Validation {
  QList<QByteArray> paths;
  QString error;
  bool overwrite = false;
};
Validation validate(Operation operation, bool directory, const QList<QByteArray>& paths, const Names& names,
                    const std::shared_ptr<std::atomic_bool>& cancelled);
}  // namespace Portal
