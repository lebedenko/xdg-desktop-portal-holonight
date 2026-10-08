#include "wire.h"

#include <QDBusMetaType>

namespace Portal {
QDBusArgument& operator<<(QDBusArgument& argument, const Pattern& value) {
  argument.beginStructure();
  argument << value.kind << value.value;
  argument.endStructure();
  return argument;
}
const QDBusArgument& operator>>(const QDBusArgument& argument, Pattern& value) {
  argument.beginStructure();
  argument >> value.kind >> value.value;
  argument.endStructure();
  return argument;
}
QDBusArgument& operator<<(QDBusArgument& argument, const Filter& value) {
  argument.beginStructure();
  argument << value.name << value.patterns;
  argument.endStructure();
  return argument;
}
const QDBusArgument& operator>>(const QDBusArgument& argument, Filter& value) {
  argument.beginStructure();
  argument >> value.name >> value.patterns;
  argument.endStructure();
  return argument;
}
QDBusArgument& operator<<(QDBusArgument& argument, const Pair& value) {
  argument.beginStructure();
  argument << value.id << value.label;
  argument.endStructure();
  return argument;
}
const QDBusArgument& operator>>(const QDBusArgument& argument, Pair& value) {
  argument.beginStructure();
  argument >> value.id >> value.label;
  argument.endStructure();
  return argument;
}
QDBusArgument& operator<<(QDBusArgument& argument, const Choice& value) {
  argument.beginStructure();
  argument << value.id << value.label << value.values << value.selected;
  argument.endStructure();
  return argument;
}
const QDBusArgument& operator>>(const QDBusArgument& argument, Choice& value) {
  argument.beginStructure();
  argument >> value.id >> value.label >> value.values >> value.selected;
  argument.endStructure();
  return argument;
}
void registerWireTypes() {
  qDBusRegisterMetaType<Pattern>();
  qDBusRegisterMetaType<Patterns>();
  qDBusRegisterMetaType<Filter>();
  qDBusRegisterMetaType<Filters>();
  qDBusRegisterMetaType<Pair>();
  qDBusRegisterMetaType<Pairs>();
  qDBusRegisterMetaType<Choice>();
  qDBusRegisterMetaType<Choices>();
  qDBusRegisterMetaType<Names>();
}
Options decodeOptions(Operation operation, const QString& title, const QString& parent, const QVariantMap& value) {
  Options options;
  options.operation = operation;
  options.title = title;
  options.parent = parent;
  options.accept_label = value.value("accept_label", operation == Operation::Open ? "Open" : "Save").toString();
  options.modal = value.value("modal", true).toBool();
  options.directory = operation == Operation::SaveMany || value.value("directory", false).toBool();
  options.multiple = operation == Operation::Open && value.value("multiple", false).toBool();
  options.folder = decode<QByteArray>(value.value("current_folder"));
  options.file = decode<QByteArray>(value.value("current_file"));
  options.name = value.value("current_name").toString();
  options.files = decode<Names>(value.value("files"));
  options.filters = decode<Filters>(value.value("filters"));
  options.current_filter = decode<Filter>(value.value("current_filter"));
  options.choices = decode<Choices>(value.value("choices"));
  return options;
}
}  // namespace Portal
