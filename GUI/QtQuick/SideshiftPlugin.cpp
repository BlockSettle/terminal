/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SideshiftPlugin.h"
#include <qqml.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

SideshiftPlugin::SideshiftPlugin(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : Plugin(parent), logger_(logger)
{
   qmlRegisterInterface<SideshiftPlugin>("SideshiftPlugin");
   outputCurrencies_.append(QLatin1Literal("BTC"));
}

SideshiftPlugin::~SideshiftPlugin()
{
   deinit();
}

static size_t writeToString(void* ptr, size_t size, size_t count, std::string* stream)
{
   const size_t resSize = size * count;
   stream->append((char*)ptr, resSize);
   return resSize;
}

static int dumpFunc(CURL* handle, curl_infotype type,
   char* data, size_t size, void* clientp)
{
   if (!data || !size) {
      return 0;
   }
   const auto log = static_cast<spdlog::logger*>(clientp);
   if (log) {
      log->debug("[dump] {}", std::string(data, size));
   }
   return 0;
};

void SideshiftPlugin::init()
{
   deinit();
   curl_ = curl_easy_init();
   curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeToString);
   curl_easy_setopt(curl_, CURLOPT_USERAGENT, std::string("BlockSettle " + name().toStdString() + " plugin").c_str());
   curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
   curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
   curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);

   //curl_easy_setopt(curl_, CURLOPT_DEBUGFUNCTION, dumpFunc);
   //curl_easy_setopt(curl_, CURLOPT_DEBUGDATA, logger_.get());
   //curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);

   curlHeaders_ = curl_slist_append(curlHeaders_, "accept: */*");
   curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curlHeaders_);

   const auto& response = get("/coins");
   if (!response.empty()) {
      try {
         const auto& msg = json::parse(response);
         for (const auto& coin : msg) {
            const auto& currency = coin["coin"].get<std::string>();
            //if (currency != "BTC") {
               inputCurrencies_.append(QString::fromStdString(currency));
            //}
         }
         logger_->debug("[{}] {} input currencies", __func__, inputCurrencies_.size());
         emit inited();
      }
      catch (const json::exception&) {
         logger_->error("[{}] failed to parse {}", __func__, response);
      }
   }
}

void SideshiftPlugin::deinit()
{
   inputCurrencies_.clear();
   if (curlHeaders_) {
      curl_slist_free_all(curlHeaders_);
      curlHeaders_ = nullptr;
   }
   if (curl_) {
      curl_easy_cleanup(curl_);
      curl_ = NULL;
   }
}

std::string SideshiftPlugin::get(const std::string& request)
{
   if (!curl_) {
      logger_->error("[{}] curl not inited");
      return {};
   }
   const auto url = baseURL_ + request;
   curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
   std::string response;
   curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
   //long respCode = 0;
   //curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &respCode);

   const auto res = curl_easy_perform(curl_);
   if (res != CURLE_OK) {
      logger_->error("[{}] {} failed: {}", __func__, url, res);
      return {};
   }
   logger_->debug("[{}] {} response: {}", __func__, url, response);
   return response;
}

void SideshiftPlugin::inputCurrencySelected(const QString& cur)
{
   logger_->debug("{{}] {}", __func__, cur.toStdString());
   depositAddr_.clear();
   emit inputCurSelected();
}
