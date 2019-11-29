/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __LOG_MANAGER_H__
#define __LOG_MANAGER_H__

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace spdlog {
   class logger;
   namespace sinks {
      class sink;
   }
}

namespace bs {
   enum class LogLevel
   {
      trace = 0,
      debug = 1,
      info = 2,
      warn = 3,
      err = 4,
      crit = 5,
      off = 6,
   };

   struct LogConfig
   {
      std::string fileName;
      std::string pattern;
      std::string category;
      LogLevel    level;
      bool        truncate;

      LogConfig();
      LogConfig(const std::string &fn, const std::string &ptn, const std::string &cat
         , const LogLevel lvl = LogLevel::debug, bool trunc = false);
   };

   class LogManager
   {
   public:
      using OnErrorCallback = std::function<void(void)>;

      LogManager(const OnErrorCallback &cb = nullptr);

      bool add(const LogConfig &);
      void add(const std::vector<LogConfig> &);
      bool add(const std::shared_ptr<spdlog::logger> &, const std::string &category = {});

      std::shared_ptr<spdlog::logger> logger(const std::string &category = {});

      // Returns spdlog format (uses BS_LOG_FORMAT env variable if set, defaultValue otherwise)
      static std::string detectFormatOverride(const std::string &defaultValue = {});

   private:
      std::shared_ptr<spdlog::logger> create(const LogConfig &);
      std::shared_ptr<spdlog::logger> createOrAppend(const std::shared_ptr<spdlog::logger> &, const LogConfig &);
      std::shared_ptr<spdlog::logger> copy(const std::shared_ptr<spdlog::logger> &, const std::string &srcCat, const std::string &category);

   private:
      const OnErrorCallback   cb_;
      std::unordered_map<std::string, std::shared_ptr<spdlog::logger>>        loggers_;
      std::unordered_map<std::string, std::shared_ptr<spdlog::sinks::sink>>   sinks_;
      std::shared_ptr<spdlog::sinks::sink> stderrSink_;
      std::unordered_map<std::string, std::string> patterns_;
      std::shared_ptr<spdlog::logger>              defaultLogger_;
      std::shared_ptr<spdlog::logger>              stdoutLogger_;
   };

}  // namespace bs

#endif // __LOG_MANAGER_H__
