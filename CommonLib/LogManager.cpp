/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "LogManager.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#include "SystemFileUtils.h"

using namespace bs;

namespace {
   const std::string catDefault = "_default_";
   const std::string tearline = "--------------------------------------------------------------";

   const std::string DefaultFormatEnv = "BS_LOG_FORMAT";
   const std::string DefaultFormat = "%C/%m/%d %H:%M:%S.%e [%L](%t)%n: %v";

} // namespace


LogConfig::LogConfig()
   : pattern(LogManager::detectFormatOverride(DefaultFormat))
   , level(LogLevel::debug)
   , truncate(false)
{
}

LogConfig::LogConfig(const std::string &fn, const std::string &ptn, const std::string &cat, const LogLevel lvl, bool trunc)
   : fileName(fn)
   , pattern(ptn)
   , category(cat)
   , level(lvl)
   , truncate(trunc)
{
}

LogManager::LogManager(const OnErrorCallback &cb)
   : cb_(cb)
{
   std::string override = detectFormatOverride();
   if (!override.empty()) {
      spdlog::set_pattern(detectFormatOverride(DefaultFormat));
   }
   stderrSink_ = std::make_shared<spdlog::sinks::stderr_sink_mt>();
   stderrSink_->set_level(spdlog::level::err);
}

bool LogManager::add(const std::shared_ptr<spdlog::logger> &logger, const std::string &category)
{
   if (!logger) {
      return true;
   }
   bool exists = (loggers_.find(category) != loggers_.end());
   if (category.empty()) {
      exists = (defaultLogger_ != nullptr);
      defaultLogger_ = logger;
   }
   else {
      loggers_[category] = logger;
   }
   logger->info(tearline);
   return !exists;
}

bool LogManager::add(const LogConfig &config)
{
   std::shared_ptr<spdlog::logger> logger;
   try {
      logger = create(config);
   }
   catch (const spdlog::spdlog_ex &) {
      if (cb_) {
         cb_();
         try {
            logger = create(config);
         }
         catch (...) {}
      }
   }
   if (!logger) {
      return true;
   }
   return add(logger, config.category);
}

void LogManager::add(const std::vector<LogConfig> &configs)
{
   for (const auto &config : configs) {
      add(config);
   }
}

static spdlog::level::level_enum convertLevel(LogLevel level)
{
   switch (level) {
   case LogLevel::trace:   return spdlog::level::trace;
   case LogLevel::debug:   return spdlog::level::debug;
   case LogLevel::info:    return spdlog::level::info;
   case LogLevel::warn:    return spdlog::level::warn;
   case LogLevel::err:     return spdlog::level::err;
   case LogLevel::crit:    return spdlog::level::critical;
   case LogLevel::off:     return spdlog::level::off;
   }

   return spdlog::level::off;
}

std::shared_ptr<spdlog::logger> LogManager::create(const LogConfig &config)
{
   // Latest spdlog creates default logger with empty name by default (search for SPDLOG_DISABLE_DEFAULT_LOGGER).
   // In this case logger creation would fail because same name already used by spdlog.
   // Let's unregister this default logger and create new one.
   if (config.category.empty()) {
      spdlog::drop("");
   }

   std::shared_ptr<spdlog::logger> result;
   if (config.category.empty()) {
      result = createOrAppend(defaultLogger_, config);
   }
   else {
      const auto &itLogger = loggers_.find(config.category);
      result = createOrAppend((itLogger == loggers_.end()) ? nullptr : itLogger->second, config);
   }

   if (!result) {
      return nullptr;
   }
   if (!config.pattern.empty()) {
      result->set_pattern(detectFormatOverride(config.pattern));
      patterns_[config.category.empty() ? catDefault : config.category] = config.pattern;
   }
   const auto level = convertLevel(config.level);
   result->set_level(level);
   result->flush_on(level);
   return result;
}

std::shared_ptr<spdlog::logger> LogManager::createOrAppend(const std::shared_ptr<spdlog::logger> &logger, const LogConfig &config)
{
   std::shared_ptr<spdlog::logger> result;

   if (!config.fileName.empty()) {
      const auto pSep = config.fileName.find_last_of('/');
      if (pSep != std::string::npos) {
         const auto filePath = config.fileName.substr(0, pSep);
         auto logFilePath = SystemFileUtils::absolutePath(filePath);
         if (!SystemFileUtils::pathExist(logFilePath)) {
            SystemFileUtils::mkPath(logFilePath);
         }
      }
   }

   if (logger) {
      auto sinks = logger->sinks();
      if (config.fileName.empty()) {
         sinks.push_back(std::make_shared<spdlog::sinks::stdout_sink_mt>());
      }
      else {
         if (sinks_.find(config.fileName) != sinks_.end()) {
            return nullptr;
         }
         sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>(config.fileName, config.truncate));
      }
      result = std::make_shared<spdlog::logger>(config.category, std::begin(sinks), std::end(sinks));
   }
   else {
      const auto itSink = sinks_.find(config.fileName);
      if (itSink == sinks_.end()) {
         result = spdlog::basic_logger_mt(config.category, config.fileName, config.truncate);
         sinks_[config.fileName] = result->sinks()[0];
      }
      else {
         result = std::make_shared<spdlog::logger>(config.category, itSink->second);
      }
   }

#ifndef NDEBUG
   result->sinks().push_back(stderrSink_);
#endif

   return result;
}

std::shared_ptr<spdlog::logger> LogManager::copy(const std::shared_ptr<spdlog::logger> &logger, const std::string &srcCat
   , const std::string &category)
{
   const auto &sinks = logger->sinks();
   const auto &result = std::make_shared<spdlog::logger>(category, std::begin(sinks), std::end(sinks));
   result->set_level(logger->level());
   result->flush_on(logger->level());

   const auto &itPattern = patterns_.find(srcCat);
   if ((itPattern != patterns_.end()) && !itPattern->second.empty()) {
      result->set_pattern(detectFormatOverride(itPattern->second));
      patterns_[category.empty() ? catDefault : category] = itPattern->second;
   }

   add(result, category);
   return result;
}

std::shared_ptr<spdlog::logger> LogManager::logger(const std::string &category)
{
   if (category.empty()) {
      if (defaultLogger_) {
         return defaultLogger_;
      }
   }
   else {
      const auto &it = loggers_.find(category);
      if (it != loggers_.end()) {
         return it->second;
      }
      if (defaultLogger_) {
         return copy(defaultLogger_, catDefault, category);
      }
   }

   // Fix new spdlog exception trying to create logger with same name
   if (!stdoutLogger_) {
      stdoutLogger_ = spdlog::stdout_logger_mt("stdout");
   }
   return stdoutLogger_;
}

// static
std::string LogManager::detectFormatOverride(const std::string &defaultValue)
{
   const char *override = std::getenv(DefaultFormatEnv.c_str());
   return override ? override : defaultValue;
}
