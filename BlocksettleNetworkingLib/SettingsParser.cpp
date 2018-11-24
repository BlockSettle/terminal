#include "SettingsParser.h"

#include <set>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QSettings>
#include <spdlog/spdlog.h>

SettingsParser::SettingsParser(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
{
   addParam(SettingsFile, "settings_file", "", "Path to settings file");
}

bool SettingsParser::LoadSettings(const QStringList& argList)
{
   QCommandLineParser parser;

   for (SettingsParam *param : params_) {
      // Current param value is default value, show it in the help
      QString defaultValueHelp = (*param)();
      if (defaultValueHelp.isEmpty()) {
         defaultValueHelp = QLatin1String("<empty>");
      }
      QString desc = QString(QLatin1String("%1. Default: %2")).arg(param->desc()).arg(defaultValueHelp);
      parser.addOption({param->name(), desc, param->name()});
   }

   parser.addHelpOption();

   parser.process(argList);

   if (parser.isSet(SettingsFile.name())) {
      QString settingsPathValue = parser.value(SettingsFile.name());
      QFileInfo fileInfo(settingsPathValue);
      if (!fileInfo.exists()) {
         logger_->error("Settings file does not exist: {}", settingsPathValue.toStdString());
         return false;
      }

      QSettings settings(settingsPathValue, QSettings::IniFormat);

      if (settings.status() != QSettings::NoError) {
         logger_->error("Failed to parse settings file: {}", settingsPathValue.toStdString());
         return false;
      }

      // Check that settings file does not contain unknown keys
      auto allKeys = settings.allKeys();
      std::set<QString> unknownKeys(allKeys.begin(), allKeys.end());

      for (SettingsParam *param : params_) {
         if (settings.contains(param->name())) {
            param->value_ = settings.value(param->name()).toString();
         }
         unknownKeys.erase(param->name());
      }

      if (!unknownKeys.empty()) {
         for (const QString& key : unknownKeys) {
            logger_->error("Unknown key '{}' in settings file '{}'"
               , key.toStdString(), settingsPathValue.toStdString());
         }
         return false;
      }
   }

   for (SettingsParam *param : params_) {
      if (parser.isSet(param->name())) {
         param->value_ = parser.value(param->name());
      }
   }

   return true;
}

void SettingsParser::addParam(SettingsParser::SettingsParam &param
   , const char *name, const char *defValue, const char *descr)
{
   param.name_ = name;
   param.value_ = QLatin1String(defValue);
   param.desc_ = descr;
   params_.push_back(&param);
}
