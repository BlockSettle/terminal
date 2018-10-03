#ifndef __SETTINGS_PARSER_H__
#define __SETTINGS_PARSER_H__

#include <memory>
#include <vector>
#include <QString>

namespace spdlog
{
class logger;
}

// Base class for command line arguments parsing.
//
// If --settings_file argument is set then settings loads from it (ini-format).
// It will be checked for unknown parameters.
// --help is generated and processed as exptected.
// See MobileAppServerSettings for usage example.
class SettingsParser
{
public:
   class SettingsParam {
   public:
      QString operator()() const { return value_; }
      QString name() const { return QLatin1String(name_); }
      QString desc() const { return QLatin1String(desc_); }

   private:
      friend class SettingsParser;
      const char* name_{};
      const char* desc_{};
      QString value_;
   };

   SettingsParam SettingsFile;

   SettingsParser(const std::shared_ptr<spdlog::logger>& logger);
   ~SettingsParser() noexcept = default;

   bool LoadSettings(const QStringList& argList);


protected:
   void addParam(SettingsParam &param, const char* name, const char* defValue, const char* descr);

   std::shared_ptr<spdlog::logger> logger_;
   std::vector<SettingsParam*> params_;
};

#endif // __SETTINGS_PARSER_H__
