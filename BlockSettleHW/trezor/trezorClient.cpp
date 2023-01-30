/*

***********************************************************************************
* Copyright (C) 2020 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "trezorClient.h"
#include <curl/curl.h>
#include "hwdevicemanager.h"
#include "trezorDevice.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "ScopeGuard.h"

using namespace bs::hww;

TrezorClient::TrezorClient(const std::shared_ptr<spdlog::logger>& logger
   , bool testNet, DeviceCallbacks* cb)
   : bs::WorkerPool(1, 1), logger_(logger), cb_(cb), testNet_(testNet)
{}

void TrezorClient::initConnection()
{
   const auto& cb = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<TrezorPostOut>(data);
      if (!reply || !reply->error.empty() || reply->response.empty()) {
         logger_->error("[TrezorClient::initConnection] network error: {}"
            , (reply && !reply->error.empty()) ? reply->error : "<empty>");
         return;
      }
      nlohmann::json response;
      try {
         response = nlohmann::json::parse(reply->response);
      }
      catch (const nlohmann::json::exception& e) {
         logger_->error("[TrezorClient::initConnection] failed to parse '{}': {}"
            , reply->response, e.what());
         return;
      }

      logger_->info("[TrezorClient::initConnection] connection inited, bridge version: {}"
         , response["version"].get<std::string>());

      state_ = trezor::State::Init;
      //emit initialized();
   };
   logger_->info("[TrezorClient::initConnection]");
   auto inData = std::make_shared<TrezorPostIn>();
   inData->path = "/";
   processQueued(inData, cb);
}

void bs::hww::TrezorClient::releaseConnection()
{
   for (const auto& device : devices_) {
      device->releaseConnection();
   }
}

std::vector<DeviceKey> TrezorClient::deviceKeys() const
{
   std::vector<DeviceKey> result;
   for (const auto& dev : devices_) {
      result.push_back(dev->key());
   }
   return result;
}

std::shared_ptr<TrezorDevice> TrezorClient::getDevice(const std::string& deviceId)
{
   for (const auto& dev : devices_) {
      if (dev->key().id == deviceId) {
         return dev;
      }
   }
   return nullptr;
}

std::shared_ptr<bs::Worker> TrezorClient::worker(const std::shared_ptr<InData>&)
{
   const std::vector<std::shared_ptr<Handler>> handlers{ std::make_shared<TrezorPostHandler>
      (logger_, trezorEndPoint_) };
   return std::make_shared<bs::WorkerImpl>(handlers);
}

void TrezorClient::listDevices()
{
   if (state_ == trezor::State::None) {
      initConnection();
   }
   const auto& cb = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<TrezorPostOut>(data);
      if (!reply || !reply->error.empty() || reply->response.empty()) {
         if (!reply) {
            logger_->error("[TrezorClient::listDevices] invalid reply type");
         }
         else {
            logger_->error("[TrezorClient::listDevices] network error: {}"
               , !reply->error.empty() ? reply->error : "<empty>");
         }
         cb_->scanningDone();
         return;
      }
      nlohmann::json response;
      try {
         response = nlohmann::json::parse(reply->response);
      }
      catch (const nlohmann::json::exception& e) {
         logger_->error("[TrezorClient::listDevices] failed to parse '{}': {}"
            , reply->response, e.what());
         cb_->scanningDone();
         return;
      }

      std::vector<trezor::DeviceData> trezorDevices;
      for (const auto& device : response) {
         const auto& session = device["session"].is_null() ? "null"
            : device["session"].get<std::string>();
         const auto& debugSession = device["debugSession"].is_null() ? "null"
            : device["debugSession"].get<std::string>();
         trezorDevices.push_back({device["path"].get<std::string>()
            , device["vendor"].get<int>(), device["product"].get<int>()
            , session, device["debug"].get<bool>(), debugSession });
      }
      logger_->info("[TrezorClient::listDevices] enumeration finished, #devices: {}"
         , trezorDevices.size());
      state_ = trezor::State::Enumerated;

      nbDevices_ = trezorDevices.size();
      if (nbDevices_ == 0) {
         cb_->scanningDone();
      }
      else {
         for (const auto& dev : trezorDevices) {
            acquireDevice(dev);
         }
      }
   };
   logger_->info("[TrezorClient::listDevices]");
   auto inData = std::make_shared<TrezorPostIn>();
   inData->path = "/enumerate";
   processQueued(inData, cb);
}

void TrezorClient::acquireDevice(const trezor::DeviceData& devData, bool init)
{
   const auto& prevSessionId = devices_.empty() ? "null"
      : devices_.at(devices_.size() - 1)->data().sessionId;
   auto inData = std::make_shared<TrezorPostIn>();
   inData->path = "/acquire/" + devData.path + "/" + prevSessionId;

   auto acquireCallback = [this, prevSessionId, devData, init]
      (const std::shared_ptr<bs::OutData>& data)
   {
      if (nbDevices_ == 0) {
         cb_->scanningDone();
      }
      --nbDevices_;
      const auto& scanDone = [this]
      {
         if (nbDevices_ == 0) {
            cb_->scanningDone();
         }
      };
      const auto& reply = std::static_pointer_cast<TrezorPostOut>(data);
      if (!reply || !reply->error.empty() || reply->response.empty()) {
         logger_->error("[TrezorClient::acquireDevice] network error: {}"
            , (reply && !reply->error.empty()) ? reply->error : "<empty>");
         scanDone();
         return;
      }
      nlohmann::json response;
      try {
         response = nlohmann::json::parse(reply->response);
      }
      catch (const nlohmann::json::exception& e) {
         logger_->error("[TrezorClient::acquireDevice] failed to parse '{}': {}", reply->response, e.what());
         scanDone();
         return;
      }

      trezor::DeviceData devData;
      devData.sessionId = response["session"].is_null() ? "null"
         : response["session"].get<std::string>();

      if (devData.sessionId.empty() || devData.sessionId == prevSessionId) {
         logger_->error("[TrezorClient::acquireDevice] cannot acquire device");
         scanDone();
         return;
      }

      logger_->info("[TrezorClient::acquireDevice] Connection has successfully acquired. Old "
         "connection id: {}, new connection id: {}", prevSessionId, devData.sessionId);

      state_ = trezor::State::Acquired;
      //emit deviceReady();
      const auto& newDevice = std::make_shared<TrezorDevice>(logger_, devData
         , testNet_, cb_, trezorEndPoint_);
      if (init) {
         newDevice->init();
      }
      devices_.push_back(newDevice);
      scanDone();
   };
   logger_->info("[TrezorClient::acquireDevice] old session id: {}", prevSessionId);
   processQueued(inData, acquireCallback);
}


namespace {
   static const std::string kBlockSettleOrigin{ "Origin: https://blocksettle.trezor.io" };
}

static size_t writeToString(void* ptr, size_t size, size_t count, std::string* stream)
{
   const size_t resSize = size * count;
   stream->append((char*)ptr, resSize);
   return resSize;
}

TrezorPostHandler::TrezorPostHandler(const std::shared_ptr<spdlog::logger>& logger
   , const std::string& baseURL)
   : logger_(logger), baseURL_(baseURL)
{
   curl_ = curl_easy_init();
   curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeToString);

   curlHeaders_ = curl_slist_append(curlHeaders_, kBlockSettleOrigin.c_str());
   //curlHeaders_ = curl_slist_append(curlHeaders_, "content-type: application/x-www-form-urlencoded;");
   curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curlHeaders_);
   curl_easy_setopt(curl_, CURLOPT_POST, 1);
}

bs::hww::TrezorPostHandler::~TrezorPostHandler()
{
   curl_slist_free_all(curlHeaders_);
   curl_easy_cleanup(curl_);
}

std::shared_ptr<TrezorPostOut> bs::hww::TrezorPostHandler::processData(const std::shared_ptr<TrezorPostIn>& inData)
{
   auto result = std::make_shared<TrezorPostOut>();
   if (!curl_) {
      result->error = "curl not inited";
      return result;
   }
   const std::string url{ baseURL_ + inData->path };
   logger_->debug("[{}] request: '{}' to {}", __func__, inData->input, url);
   curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
   if (!inData->input.empty()) {
      curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, inData->input.data());
   }
   if (inData->timeout) {
      curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, 3000L);
   }
   else {
      curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 0L);
   }

   std::string response;
   curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

   const auto res = curl_easy_perform(curl_);
   if (res != CURLE_OK) {
      result->error = fmt::format("failed to post to {}: {}", url, res);
      result->timedOut = (res == CURLE_OPERATION_TIMEDOUT);
      return result;
   }
   result->response = response;
   logger_->debug("[{}] response: {}", __func__, result->response);
   return result;
}
