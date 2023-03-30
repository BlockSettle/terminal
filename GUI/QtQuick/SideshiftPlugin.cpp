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
#include <QQuickImageProvider>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

class CoinImageProvider : public QQuickImageProvider
{
public:
   CoinImageProvider(SideshiftPlugin* parent)
      : QQuickImageProvider(QQuickImageProvider::Pixmap), parent_(parent)
   {}
   QPixmap requestPixmap(const QString& id, QSize* size, const QSize& requestedSize) override;

private:
   SideshiftPlugin* parent_;
};


SideshiftPlugin::SideshiftPlugin(const std::shared_ptr<spdlog::logger>& logger
   , QQmlApplicationEngine& engine, QObject* parent)
   : Plugin(parent), logger_(logger)
{
   qmlRegisterInterface<SideshiftPlugin>("SideshiftPlugin");
   engine.addImageProvider(QLatin1Literal("coin"), new CoinImageProvider(this));
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
            auto& networks = networksByCur_[currency];
            for (const auto& network : coin["networks"]) {
               networks.append(QString::fromStdString(network.get<std::string>()));
            }
            inputCurrencies_.append(QString::fromStdString(currency));
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
   networksByCur_.clear();
   inputCurrencies_.clear();
   inputNetworks_.clear();
   inputNetwork_.clear();
   inputCurrency_.clear();
   convRate_.clear();
   orderId_.clear();

   if (curlHeaders_) {
      curl_slist_free_all(curlHeaders_);
      curlHeaders_ = nullptr;
   }
   if (curl_) {
      curl_easy_cleanup(curl_);
      curl_ = NULL;
   }
}

void SideshiftPlugin::setInputNetwork(const QString& network)
{
   logger_->debug("[{}] {} '{}' -> '{}'", __func__, inputCurrency_.toStdString()
      , inputNetwork_.toStdString(), network.toStdString());
   if (inputCurrency_.isEmpty() || (inputNetwork_ == network)) {
      return;
   }
   inputNetwork_ = network;
   const auto& response = get("/pair/" + inputCurrency_.toStdString() + "-"
      + network.toStdString() + "/btc-bitcoin");
   try {
      const auto& respJson = json::parse(response);
      double rate = 0;
      if (respJson["rate"].is_string()) {
         rate = std::stod(respJson["rate"].get<std::string>());
      }
      else if (respJson["rate"].is_number_float()) {
         rate = respJson["rate"].get<double>();
      }
      if (rate > 0) {
         if (rate < 0.0001) {
            convRate_ = tr("1 BTC = %1 %2").arg(QString::number(1.0 / rate, 'f', 2))
               .arg(inputCurrency_.toUpper());
         }
         else {
            convRate_ = tr("1 %1 = %2 BTC").arg(inputCurrency_.toUpper())
               .arg(QString::number(rate, 'f', 6));
         }
      }
      minAmount_ = QString::fromStdString(respJson["min"].get<std::string>());
      maxAmount_ = QString::fromStdString(respJson["max"].get<std::string>());
   }
   catch (const json::exception& e) {
      logger_->error("[{}] failed to parse {}: {}", __func__, response, e.what());
   }
   emit inputSelected();
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

std::string SideshiftPlugin::post(const std::string& path, const std::string& data)
{
   return std::string();
}

void SideshiftPlugin::inputCurrencySelected(const QString& cur)
{
   logger_->debug("{{}] '{}' -> '{}'", __func__, inputCurrency_.toStdString()
      , cur.toStdString());
   if (inputCurrency_ == cur) {
      return;
   }
   inputCurrency_ = cur;
   try {
      inputNetworks_ = networksByCur_.at(cur.toStdString());
   }
   catch (const std::exception&) {
      inputNetworks_.clear();
   }
   emit inputCurSelected();
}

static QString statusToQString(const std::string& s)
{  //TODO: should be translatable at some point
   return QString::fromStdString(s);
}

bool SideshiftPlugin::sendShift(const QString& recvAddr)
{
   if (inputCurrency_.isEmpty() || inputNetwork_.isEmpty() || recvAddr.isEmpty()) {
      logger_->error("[{}] invalid input data: inCur: '{}', inNet: '{}', recvAddr: '{}'"
         , __func__, inputCurrency_.toStdString(), inputNetwork_.toStdString(), recvAddr.toStdString());
      return false;
   }
   const json msgReq{ {"settleAddress", recvAddr.toStdString()}, {"affiliateId", affiliateId_}
   , {"settleCoin", "btc"}, {"depositCoin", inputCurrency_.toStdString()}
   , {"depositNetwork", inputNetwork_.toStdString()} };
   const auto& response = post("/shifts/variable", msgReq.dump());
   try {
      const auto& msgResp = json::parse(response);
      orderId_ = QString::fromStdString(msgResp["id"].get<std::string>());
      creationDate_ = QString::fromStdString(msgResp["createdAt"].get<std::string>());
      expireDate_ = QString::fromStdString(msgResp["expiresAt"].get<std::string>());
      depositAddr_ = QString::fromStdString(msgResp["depositAddress"].get<std::string>());
      shiftStatus_ = statusToQString(msgResp["status"].get<std::string>());
   }
   catch (const std::exception& e) {
      logger_->error("[{}] failed to parse {}: {}", __func__, response, e.what());
      return false;
   }
   emit orderSent();
   return true;
}

void SideshiftPlugin::updateShiftStatus()
{
   if (orderId_.isEmpty()) {
      return;
   }
   const auto& response = get("/shifts/" + orderId_.toStdString());
   try {
      const auto& msgResp = json::parse(response);
      shiftStatus_ = statusToQString(msgResp["status"].get<std::string>());
      emit orderSent();
   }
   catch (const std::exception& e) {
      logger_->error("[{}] failed to parse {}: {}", __func__, response, e.what());
   }
}


#include <QSvgRenderer>
#include <QPainter>
QPixmap CoinImageProvider::requestPixmap(const QString& id, QSize* size
   , const QSize& requestedSize)
{
   const std::string request = "/coins/icon/" + id.toStdString();
   const auto& response = parent_->get(request);
   parent_->logger_->debug("[{}] {} returned {} bytes", __func__, request
      , response.size());
   if (response.empty()) {
      return {};
   }
   if (response.at(0) == '{') {  //likely an error in json
      return {};
   }
   QSvgRenderer r(QByteArray::fromStdString(response));
   QImage img(requestedSize, QImage::Format_ARGB32);
   img.fill(Qt::GlobalColor::transparent);
   QPainter p(&img);
   r.render(&p);
   if (size) {
      *size = requestedSize;
   }
   return QPixmap::fromImage(img);
}
