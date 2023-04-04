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

namespace {
   const QHash<int, QByteArray> kCurrencyListRoles {
      {CurrencyListModel::CurrencyRoles::NameRole, "name"},
      {CurrencyListModel::CurrencyRoles::CoinRole, "coin"},
      {CurrencyListModel::CurrencyRoles::NetworkRole, "network"}
   };
}

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

CurrencyListModel::CurrencyListModel(QObject* parent)
   : QAbstractListModel(parent)
{
}

int CurrencyListModel::rowCount(const QModelIndex&) const
{
   return currencies_.size();
}

QVariant CurrencyListModel::data(const QModelIndex& index, int role) const
{
   if (index.row() < 0 || index.row() >= currencies_.size()) {
      return QVariant();
   }
   switch(role) {
      case CurrencyListModel::CurrencyRoles::NameRole: return currencies_.at(index.row()).name;
      case CurrencyListModel::CurrencyRoles::CoinRole: return currencies_.at(index.row()).coin;
      case CurrencyListModel::CurrencyRoles::NetworkRole: return currencies_.at(index.row()).network;
      default: return QVariant();
   }
   return QVariant();
}

QHash<int, QByteArray> CurrencyListModel::roleNames() const
{
   return kCurrencyListRoles;
}

void CurrencyListModel::reset(const QList<Currency>& currencies)
{
   beginResetModel();
   currencies_ = currencies;
   endResetModel();
}

CurrencyFilterModel::CurrencyFilterModel(QObject* parent)
   : QSortFilterProxyModel(parent)
{
   connect(this, &CurrencyFilterModel::changed, this, &CurrencyFilterModel::invalidate);
}

const QString& CurrencyFilterModel::filter() const
{
   return filter_;
}

void CurrencyFilterModel::setFilter(const QString& filter)
{
   if (filter_ != filter) {
      filter_ = filter;
      emit changed();
   }
}

bool CurrencyFilterModel::filterAcceptsRow(int source_row,
      const QModelIndex& source_parent) const
{
   if (filter_.length() == 0) {
      return true;
   }

   return (sourceModel()->data(index(source_row, 0), CurrencyListModel::CurrencyRoles::NameRole).toString().contains(filter_)
      || sourceModel()->data(index(source_row, 0), CurrencyListModel::CurrencyRoles::CoinRole).toString().contains(filter_)
      || sourceModel()->data(index(source_row, 0), CurrencyListModel::CurrencyRoles::NetworkRole).toString().contains(filter_));
}
   
bool CurrencyFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
   return sourceModel()->data(index(left.row(), 0), CurrencyListModel::CurrencyRoles::NameRole) <
      sourceModel()->data(index(right.row(), 0), CurrencyListModel::CurrencyRoles::NameRole);
}

struct PostIn : public bs::InData
{
   ~PostIn() override = default;
   std::string path;
   std::string data;
};
struct PostOut : public bs::OutData
{
   ~PostOut() override = default;
   std::string    response;
   std::string    error;
};

class SideshiftPlugin::PostHandler : public bs::HandlerImpl<PostIn, PostOut>
{
public:
   PostHandler(SideshiftPlugin* parent)
      : parent_(parent)
   {}
   ~PostHandler() override = default;

protected:
   std::shared_ptr<PostOut> processData(const std::shared_ptr<PostIn>& in) override
   {
      std::unique_lock<std::mutex> lock(parent_->curlMtx_);
      if (!parent_->curl_) {
         return nullptr;
      }
      auto out = std::make_shared<PostOut>();
      curl_easy_setopt(parent_->curl_, CURLOPT_POST, 1);
      const auto url = parent_->baseURL_ + in->path;
      curl_easy_setopt(parent_->curl_, CURLOPT_URL, url.c_str());
      curl_easy_setopt(parent_->curl_, CURLOPT_POSTFIELDS, in->data.data());
      std::string response;
      curl_easy_setopt(parent_->curl_, CURLOPT_WRITEDATA, &response);

      const auto res = curl_easy_perform(parent_->curl_);
      if (res != CURLE_OK) {
         out->error = fmt::format("{} failed: {}", url, res);
         parent_->logger_->error("[{}] {}", __func__, out->error);
         return out;
      }
      out->response = std::move(response);
      parent_->logger_->debug("[{}] {} response: {} [{}]", __func__, url, response, response.size());
      return out;
   }

private:
   SideshiftPlugin* parent_{ nullptr };
};

struct GetIn : public bs::InData
{
   ~GetIn() override = default;
   std::string path;
};
struct GetOut : public bs::OutData
{
   ~GetOut() override = default;
   std::string    response;
};

class SideshiftPlugin::GetHandler : public bs::HandlerImpl<GetIn, GetOut>
{
public:
   GetHandler(SideshiftPlugin* parent) : parent_(parent) {}
   ~GetHandler() override = default;

protected:
   std::shared_ptr<GetOut> processData(const std::shared_ptr<GetIn>& in) override
   {
      if (!parent_->curl_) {
         return nullptr;
      }
      auto out = std::make_shared<GetOut>();
      out->response = parent_->get(in->path);
      return out;
   }

private:
   SideshiftPlugin* parent_{ nullptr };
};


SideshiftPlugin::SideshiftPlugin(const std::shared_ptr<spdlog::logger>& logger
   , QQmlApplicationEngine& engine, QObject* parent)
   : Plugin(parent), bs::WorkerPool(1, 1), logger_(logger)
{
   inputListModel_ = new CurrencyListModel(this);
   inputFilterModel_ = new CurrencyFilterModel(this);
   inputFilterModel_->setSourceModel(inputListModel_);

   outputListModel_ = new CurrencyListModel(this);
   outputFilterModel_ = new CurrencyFilterModel(this);
   outputFilterModel_->setSourceModel(outputListModel_);

   qmlRegisterInterface<SideshiftPlugin>("SideshiftPlugin");
   qmlRegisterInterface<CurrencyListModel>("CurrencyListModel");
   qmlRegisterInterface<CurrencyFilterModel>("CurrencyFilterModel");
   engine.addImageProvider(QLatin1Literal("coin"), new CoinImageProvider(this));
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
   {
      std::unique_lock<std::mutex> lock(curlMtx_);
      curl_ = curl_easy_init();
      curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeToString);
      curl_easy_setopt(curl_, CURLOPT_USERAGENT, std::string("BlockSettle " + name().toStdString() + " plugin").c_str());
      curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
      curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
      curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);

      //curl_easy_setopt(curl_, CURLOPT_DEBUGFUNCTION, dumpFunc);
      //curl_easy_setopt(curl_, CURLOPT_DEBUGDATA, logger_.get());
      //curl_easy_setopt(curl_, CURLOPT_VERBOSE, 1L);

      //curlHeaders_ = curl_slist_append(curlHeaders_, "accept: */*");
      curlHeaders_ = curl_slist_append(curlHeaders_, "Content-Type: application/json");
      curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curlHeaders_);
   }

   auto in = std::make_shared<GetIn>();
   in->path = "/coins";
   const auto& getResult = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<GetOut>(data);
      if (!reply || reply->response.empty()) {
         logger_->error("[SideshiftPlugin::init] network error");
         return;
      }
      const auto& response = reply->response;
      try {
         const auto& msg = json::parse(response);
         QList<Currency> currencies;
         for (const auto& coin : msg) {
            for (const auto& network : coin["networks"]) {
               currencies.append({
                  QString::fromStdString(coin["name"]),
                  QString::fromStdString(coin["coin"]),
                  QString::fromStdString(network.get<std::string>())
               });
            }
         }

         inputListModel_->reset(currencies);
         outputListModel_->reset({ {tr("Bitcoin"), tr("BTC"), tr("")} });

         emit inited();
         logger_->debug("[SideshiftPlugin::init] {} input currencies", currencies.size());
      }
      catch (const json::exception&) {
         logger_->error("[SideshiftPlugin::init] failed to parse {}", response);
      }
   };
   processQueued(in, getResult);
}

void SideshiftPlugin::deinit()
{
   cancel();
   inputNetwork_.clear();
   inputCurrency_.clear();
   convRate_.clear();
   orderId_.clear();

   std::unique_lock<std::mutex> lock(curlMtx_);
   if (curlHeaders_) {
      curl_slist_free_all(curlHeaders_);
      curlHeaders_ = nullptr;
   }
   if (curl_) {
      curl_easy_cleanup(curl_);
      curl_ = NULL;
   }
}

static QString getConvRate(const json& msg, const QString& inputCur)
{
   double rate = 0;
   if (msg["rate"].is_string()) {
      rate = std::stod(msg["rate"].get<std::string>());
   }
   else if (msg["rate"].is_number_float()) {
      rate = msg["rate"].get<double>();
   }
   if (rate > 0) {
      if (rate < 0.0001) {
         return QObject::tr("1 BTC = %1 %2").arg(QString::number(1.0 / rate, 'f', 2))
            .arg(inputCur);
      }
      else {
         return QObject::tr("1 %1 = %2 BTC").arg(inputCur)
            .arg(QString::number(rate, 'f', 6));
      }
   }
   return {};
}

void SideshiftPlugin::getPair()
{
   auto in = std::make_shared<GetIn>();
   in->path = "/pair/" + inputCurrency_.toStdString() + "-"
      + inputNetwork_.toStdString() + "/btc-bitcoin";
   const auto& getResult = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<GetOut>(data);
      if (!reply || reply->response.empty()) {
         logger_->error("[SideshiftPlugin::getPair] network error");
         return;
      }
      const auto& response = reply->response;
      try {
         const auto& respJson = json::parse(response);
         if (respJson.contains("error")) {
            const auto& errorMsg = respJson["error"]["message"].get<std::string>();
            logger_->error("[{}] {}", __func__, errorMsg);
            convRate_ = tr("Error: %1").arg(QString::fromStdString(errorMsg));
            emit pairUpdated();
            return;
         }
         convRate_ = getConvRate(respJson, inputCurrency_.toUpper());
         minAmount_ = QString::fromStdString(respJson["min"].get<std::string>());
         maxAmount_ = QString::fromStdString(respJson["max"].get<std::string>());
      }
      catch (const json::exception& e) {
         logger_->error("[{}] failed to parse {}: {}", __func__, response, e.what());
      }
      emit pairUpdated();
   };
   processQueued(in, getResult);
}

void SideshiftPlugin::setInputNetwork(const QString& network)
{
   logger_->debug("[{}] {} '{}' -> '{}'", __func__, inputCurrency_.toStdString()
      , inputNetwork_.toStdString(), network.toStdString());
   if (inputCurrency_.isEmpty() || (inputNetwork_ == network)) {
      return;
   }
   inputNetwork_ = network;
   getPair();
}

std::string SideshiftPlugin::get(const std::string& request)
{
   std::unique_lock<std::mutex> lock(curlMtx_);
   if (!curl_) {
      logger_->error("[{}] curl not inited");
      return {};
   }
   curl_easy_setopt(curl_, CURLOPT_POST, 0);
   const auto url = baseURL_ + request;
   curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
   std::string response;
   curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

   const auto res = curl_easy_perform(curl_);
   if (res != CURLE_OK) {
      logger_->error("[{}] {} failed: {}", __func__, url, res);
      return {};
   }
   //logger_->debug("[{}] {} response: {} [{}]", __func__, url, response, response.size());
   return response;
}

void SideshiftPlugin::inputCurrencySelected(const QString& cur)
{
   logger_->debug("{{}] '{}' -> '{}'", __func__, inputCurrency_.toStdString()
      , cur.toStdString());
   if (inputCurrency_ == cur) {
      return;
   }
   inputCurrency_ = cur;
   emit inputCurSelected();
}

QString SideshiftPlugin::statusToQString(const std::string& s) const
{
   if (s == "waiting") {
      return tr("WAITING FOR YOU TO SEND %1").arg(inputCurrency_.toUpper());
   }
   return QString::fromStdString(s);
}

bool SideshiftPlugin::sendShift(const QString& recvAddr)
{
   if (inputCurrency_.isEmpty() || inputNetwork_.isEmpty() || recvAddr.isEmpty()) {
      logger_->error("[{}] invalid input data: inCur: '{}', inNet: '{}', recvAddr: '{}'"
         , __func__, inputCurrency_.toStdString(), inputNetwork_.toStdString(), recvAddr.toStdString());
      return false;
   }
   shiftStatus_ = tr("Sending request...");
   const json msgReq{ {"settleAddress", recvAddr.toStdString()}, {"affiliateId", affiliateId_}
   , {"settleCoin", "btc"}, {"depositCoin", inputCurrency_.toLower().toStdString()}
   , {"depositNetwork", inputNetwork_.toStdString()} };
   auto in = std::make_shared<PostIn>();
   in->path = "/shifts/variable";
   in->data = msgReq.dump();
   const auto& postResult = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<PostOut>(data);
      if (!reply || reply->response.empty()) {
         logger_->error("[SideshiftPlugin::sendShift] network error {}"
            , (reply == nullptr) ? "<null>" : reply->error);
         return;
      }
      const auto& response = reply->response;
      try {
         const auto& msgResp = json::parse(response);
         if (msgResp.contains("error")) {
            const auto& errMsg = msgResp["error"]["message"].get<std::string>();
            logger_->error("[SideshiftPlugin::sendShift] failed: {}", errMsg);
            shiftStatus_ = QString::fromStdString(errMsg);
         }
         else {
            orderId_ = QString::fromStdString(msgResp["id"].get<std::string>());
            creationDate_ = QString::fromStdString(msgResp["createdAt"].get<std::string>());
            expireDate_ = QString::fromStdString(msgResp["expiresAt"].get<std::string>());
            depositAddr_ = QString::fromStdString(msgResp["depositAddress"].get<std::string>());
            shiftStatus_ = statusToQString(msgResp["status"].get<std::string>());
         }
         emit orderSent();
      }
      catch (const std::exception& e) {
         logger_->error("[SideshiftPlugin::sendShift] failed to parse {}: {}"
            , response, e.what());
      }
   };
   processQueued(in, postResult);
   return true;
}

void SideshiftPlugin::updateShiftStatus()
{
   if (orderId_.isEmpty()) {
      return;
   }
   auto in = std::make_shared<GetIn>();
   in->path = "/shifts/" + orderId_.toStdString();
   const auto& getResult = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<GetOut>(data);
      if (!reply || reply->response.empty()) {
         logger_->error("[SideshiftPlugin::updateShiftStatus] network error");
         return;
      }
      const auto& response = reply->response;
      try {
         logger_->debug("[SideshiftPlugin::updateShiftStatus] {}", response);
         const auto& msgResp = json::parse(response);
         if (msgResp.contains("error")) {
            return;
         }
         shiftStatus_ = statusToQString(msgResp["status"].get<std::string>());
         emit orderSent();
      }
      catch (const std::exception& e) {
         logger_->error("[{}] failed to parse {}: {}", __func__, response, e.what());
      }
   };
   processQueued(in, getResult);
   getPair();
}

std::shared_ptr<bs::Worker> SideshiftPlugin::worker(const std::shared_ptr<bs::InData>&)
{
   const std::vector<std::shared_ptr<bs::Handler>> handlers {
      std::make_shared<PostHandler>(this), std::make_shared<GetHandler>(this) };
   return std::make_shared<bs::WorkerImpl>(handlers);
}


#include <QSvgRenderer>
#include <QPainter>
QPixmap CoinImageProvider::requestPixmap(const QString& id, QSize* size
   , const QSize& requestedSize)
{
   const std::string request = "/coins/icon/" + id.toStdString();
   const auto& response = parent_->get(request);
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
