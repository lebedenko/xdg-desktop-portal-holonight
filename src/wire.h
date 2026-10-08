#pragma once

#include <QDBusArgument>
#include <QVariantMap>

#include <cstdint>

namespace Portal {
struct Pattern {
  uint kind = 0;
  QString value;
  bool operator==(const Pattern&) const = default;
};
using Patterns = QList<Pattern>;
struct Filter {
  QString name;
  Patterns patterns;
  bool operator==(const Filter&) const = default;
};
using Filters = QList<Filter>;
struct Pair {
  QString id;
  QString label;
  bool operator==(const Pair&) const = default;
};
using Pairs = QList<Pair>;
struct Choice {
  QString id;
  QString label;
  Pairs values;
  QString selected;
};
using Choices = QList<Choice>;
using Names = QList<QByteArray>;
QDBusArgument& operator<<(QDBusArgument& argument, const Pattern& value);
const QDBusArgument& operator>>(const QDBusArgument& argument, Pattern& value);
QDBusArgument& operator<<(QDBusArgument& argument, const Filter& value);
const QDBusArgument& operator>>(const QDBusArgument& argument, Filter& value);
QDBusArgument& operator<<(QDBusArgument& argument, const Pair& value);
const QDBusArgument& operator>>(const QDBusArgument& argument, Pair& value);
QDBusArgument& operator<<(QDBusArgument& argument, const Choice& value);
const QDBusArgument& operator>>(const QDBusArgument& argument, Choice& value);
void registerWireTypes();
template <typename T>
T decode(const QVariant& value) {
  if (value.metaType() == QMetaType::fromType<QDBusArgument>()) {
    return qdbus_cast<T>(value.value<QDBusArgument>());
  }
  return value.value<T>();
}
enum class Operation : std::uint8_t { Open, Save, SaveMany };
struct Options {
  Operation operation = Operation::Open;
  QString title;
  QString parent;
  QString accept_label;
  bool modal = true;
  bool directory = false;
  bool multiple = false;
  QByteArray folder;
  QByteArray file;
  QString name;
  Names files;
  Filters filters;
  Filter current_filter;
  Choices choices;
};
Options decodeOptions(Operation operation, const QString& title, const QString& parent, const QVariantMap& values);
}  // namespace Portal
Q_DECLARE_METATYPE(Portal::Pattern)
Q_DECLARE_METATYPE(Portal::Patterns)
Q_DECLARE_METATYPE(Portal::Filter)
Q_DECLARE_METATYPE(Portal::Filters)
Q_DECLARE_METATYPE(Portal::Pair)
Q_DECLARE_METATYPE(Portal::Pairs)
Q_DECLARE_METATYPE(Portal::Choice)
Q_DECLARE_METATYPE(Portal::Choices)
Q_DECLARE_METATYPE(Portal::Names)
