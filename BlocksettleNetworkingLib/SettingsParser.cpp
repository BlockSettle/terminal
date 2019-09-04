#include "SettingsParser.h"

#include <set>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QSettings>
#include <spdlog/spdlog.h>

SettingsParser::SettingsParser(const std::shared_ptr<spdlog::logger> &logger
   , const std::string &settingsFile)
   : logger_(logger)
{
   addParam(SettingsFile, "settings_file", settingsFile.c_str(), "Path to settings file");
}

bool SettingsParser::LoadSettings(const QStringList& argList)
{
   QCommandLineParser parser;

   for (BaseSettingsParam *param : params_) {
      // Current param value is default value, show it in the help
      QString defaultValueHelp = param->defValue();
      if (defaultValueHelp.isEmpty()) {
         defaultValueHelp = QLatin1String("<empty>");
      }
      QString desc = QStringLiteral("%1. Default: %2").arg(param->desc()).arg(defaultValueHelp);

      parser.addOption({ param->name(), desc, param->name() });
   }

   parser.addHelpOption();

   parser.process(argList);

   auto unsetRequired = requiredParams_;

   if (parser.isSet(SettingsFile.name())) {
      QString settingsPathValue = parser.value(SettingsFile.name());
      QFileInfo fileInfo(settingsPathValue);
      if (!fileInfo.exists()) {
         SPDLOG_LOGGER_ERROR(logger_, "Settings file does not exist: {}", settingsPathValue.toStdString());
         return false;
      }

      QSettings settings(settingsPathValue, QSettings::IniFormat);

      if (settings.status() != QSettings::NoError) {
         SPDLOG_LOGGER_ERROR(logger_,"Failed to parse settings file: {}", settingsPathValue.toStdString());
         return false;
      }

      // Check that settings file does not contain unknown keys
      auto allKeys = settings.allKeys();
      std::set<QString> unknownKeys(allKeys.begin(), allKeys.end());

      for (BaseSettingsParam *param : params_) {
         if (settings.contains(param->name())) {
            bool result = param->setValue(settings.value(param->name()));
            if (!result) {
               return false;
            }
            unsetRequired.erase(param);
         }
         unknownKeys.erase(param->name());
      }

      if (!unknownKeys.empty()) {
         for (const QString& key : unknownKeys) {
            SPDLOG_LOGGER_ERROR(logger_,"Unknown key '{}' in settings file '{}'"
               , key.toStdString(), settingsPathValue.toStdString());
         }
         return false;
      }
   }

   for (BaseSettingsParam *param : params_) {
      if (parser.isSet(param->name())) {
         QVariant value = parser.value(param->name());
         bool result = param->setValue(value);
         if (!result) {
            SPDLOG_LOGGER_ERROR(logger_,"invalid value '{}' for key '{}' in settings file"
               , value.toString().toStdString(), param->name().toStdString());
            return false;
         }
         unsetRequired.erase(param);
      }
   }

   if (!unsetRequired.empty()) {
      for (auto param : unsetRequired) {
         SPDLOG_LOGGER_ERROR(logger_, "required parameter {} is not set", param->name().toStdString());
      }
      return false;
   }

   return true;
}

void SettingsParser::addParamVariant(SettingsParser::BaseSettingsParam &param
   , const char *name, const QVariant &defValue, const char *descr)
{
   if (paramsNames_.find(name) != paramsNames_.end()) {
      throw std::logic_error(fmt::format("duplicated parameter name detected ({})", name));
   }
   param.name_ = QLatin1String(name);
   param.defValue_ = defValue;
   param.desc_ = QLatin1String(descr);
   param.setValue(defValue);
   params_.push_back(&param);
   paramsNames_.insert(name);
}
