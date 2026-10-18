#include "settings.h"

#include "wire.h"

#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMetaType>
#include <QLoggingCategory>

#include <algorithm>
#include <holonight/theme_catalog.h>

Q_LOGGING_CATEGORY(lcSettingsPortalBackend, "holonight.portal.settingsbackend")

namespace {
constexpr auto kAppearanceNs = "org.freedesktop.appearance";
constexpr auto kColorSchemeKey = "color-scheme";
constexpr auto kAccentColorKey = "accent-color";
constexpr auto kContrastKey = "contrast";
constexpr auto kReducedMotionKey = "reduced-motion";

}  // namespace

QDBusArgument& operator<<(QDBusArgument& argument, const SettingsPortalAccentColor& color) {
  argument.beginStructure();
  argument << color.red << color.green << color.blue;
  argument.endStructure();
  return argument;
}

const QDBusArgument& operator>>(const QDBusArgument& argument, SettingsPortalAccentColor& color) {
  argument.beginStructure();
  argument >> color.red >> color.green >> color.blue;
  argument.endStructure();
  return argument;
}

SettingsPortalBackend::SettingsPortalBackend(const Holonight::ResolvedAppearance& appearance, QObject* parent)
    : QObject(parent) {
  qRegisterMetaType<SettingsPortalAccentColor>("SettingsPortalAccentColor");
  qDBusRegisterMetaType<SettingsPortalAccentColor>();
  values_ = valuesForAppearance(appearance);
}

QDBusVariant SettingsPortalBackend::Read(const QString& portal_namespace, const QString& key) const {
  if (portal_namespace != QLatin1String(kAppearanceNs)) {
    qCWarning(lcSettingsPortalBackend) << "Unsupported portal setting namespace requested:" << portal_namespace << key;
    return {};
  }
  if (key == QLatin1String(kColorSchemeKey)) {
    return variantForColorScheme(values_.color_scheme);
  }
  if (key == QLatin1String(kAccentColorKey)) {
    return variantForAccentColor(values_.accent_color);
  }
  if (key == QLatin1String(kContrastKey) || key == QLatin1String(kReducedMotionKey)) {
    return QDBusVariant{QVariant::fromValue(0U)};
  }
  qCWarning(lcSettingsPortalBackend) << "Unsupported portal setting requested:" << portal_namespace << key;
  return {};
}

QMap<QString, QVariantMap> SettingsPortalBackend::ReadAll(const QStringList& namespaces) const {
  QMap<QString, QVariantMap> result;
  if (namespaces.isEmpty() || namespaces.contains(QLatin1String(kAppearanceNs))) {
    result.insert(QLatin1String(kAppearanceNs), appearanceMap(values_));
  }
  return result;
}

void SettingsPortalBackend::applyAppearance(const Holonight::ResolvedAppearance& appearance) {
  applyValues(valuesForAppearance(appearance), true);
}

void SettingsPortalBackend::applyValues(const Values& values, bool emit_changes) {
  const Values previous = values_;
  values_ = values;
  if (!emit_changes) {
    return;
  }
  if (values_.color_scheme != previous.color_scheme) {
    emit SettingChanged(QLatin1String(kAppearanceNs), QLatin1String(kColorSchemeKey),
                        variantForColorScheme(values_.color_scheme));
  }
  if (values_.accent_color != previous.accent_color) {
    emit SettingChanged(QLatin1String(kAppearanceNs), QLatin1String(kAccentColorKey),
                        variantForAccentColor(values_.accent_color));
  }
}

SettingsPortalBackend::Values SettingsPortalBackend::valuesForAppearance(
    const Holonight::ResolvedAppearance& appearance) {
  Values values;
  values.color_scheme = appearance.color_mode == Holonight::ColorMode::Light ? kColorSchemeLight : kColorSchemeDark;
  values.accent_color = Holonight::accentColorForScheme(appearance.scheme, appearance.accent);
  return values;
}

QDBusVariant SettingsPortalBackend::variantForColorScheme(int color_scheme) {
  return QDBusVariant{QVariant::fromValue(static_cast<uint>(color_scheme))};
}

QDBusVariant SettingsPortalBackend::variantForAccentColor(const QColor& color) {
  const SettingsPortalAccentColor accent{
      .red = std::clamp(static_cast<double>(color.redF()), 0.0, 1.0),
      .green = std::clamp(static_cast<double>(color.greenF()), 0.0, 1.0),
      .blue = std::clamp(static_cast<double>(color.blueF()), 0.0, 1.0),
  };
  return QDBusVariant{QVariant::fromValue(accent)};
}

QVariantMap SettingsPortalBackend::appearanceMap(const Values& values) {
  QVariantMap map;
  map.insert(QLatin1String(kColorSchemeKey), QVariant::fromValue(variantForColorScheme(values.color_scheme)));
  map.insert(QLatin1String(kAccentColorKey), QVariant::fromValue(variantForAccentColor(values.accent_color)));
  map.insert(QLatin1String(kContrastKey), QVariant::fromValue(QDBusVariant{QVariant::fromValue(0U)}));
  map.insert(QLatin1String(kReducedMotionKey), QVariant::fromValue(QDBusVariant{QVariant::fromValue(0U)}));
  return map;
}

bool SettingsPortalBackend::handleMessage(const QDBusMessage& message, const QDBusConnection& connection) const {
  const auto args = message.arguments();
  if (message.member() == "Read" && message.signature() == "ss") {
    const auto value = Read(args[0].toString(), args[1].toString());
    if (value.variant().isValid()) {
      connection.send(message.createReply({QVariant::fromValue(value)}));
    } else {
      const QString error = args[0].toString() == "org.freedesktop.appearance"
                                ? "Requested setting not found"
                                : "Requested setting namespace not found";
      connection.send(message.createErrorReply("org.freedesktop.portal.Error.NotFound", error));
    }
  } else if (message.member() == "ReadAll" && message.signature() == "as") {
    connection.send(message.createReply({QVariant::fromValue(ReadAll(Portal::decode<QStringList>(args[0])))}));
  } else if (message.member() == "Read" || message.member() == "ReadAll") {
    connection.send(message.createErrorReply(QDBusError::InvalidArgs, "Invalid Settings arguments."));
  } else {
    return false;
  }
  return true;
}
