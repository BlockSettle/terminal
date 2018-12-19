#ifndef __SETTINGS_PARSER_H__
#define __SETTINGS_PARSER_H__

#include <memory>
#include <vector>
#include <QString>

class QVariant;
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

   protected:
      friend class SettingsParser;

      virtual bool setValue(const QVariant &value);

      const char* name_{};
      const char* desc_{};

      QString value_;
   };

   class IntSettingsParam : public SettingsParam {
   public:
      int operator()() const { return value_; }

   protected:
      bool setValue(const QVariant &value) override;

      int value_{};
   };

   class BoolSettingsParam : public SettingsParam {
   public:
      bool operator()() const { return value_; }

   protected:
      bool setValue(const QVariant &value) override;

      bool value_{};
   };

   SettingsParam SettingsFile;

   SettingsParser(const std::shared_ptr<spdlog::logger>& logger);
   ~SettingsParser() noexcept = default;

   bool LoadSettings(const QStringList& argList);

protected:
   void addParam(SettingsParam &param, const char* name, const char* defValue, const char* descr);
   void addParam(SettingsParam &param, const char* name, int defValue, const char* descr);
   void addParam(SettingsParam &param, const char* name, bool defValue, const char* descr);

   std::shared_ptr<spdlog::logger> logger_;
   std::vector<SettingsParam*> params_;
};

#endif // __SETTINGS_PARSER_H__
