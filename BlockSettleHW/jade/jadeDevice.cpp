/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QCborArray>
#include <QCborValue>
#include <QCryptographicHash>
#include <QDataStream>
#include <curl/curl.h>
#include <spdlog/spdlog.h>
#include "hwdevicemanager.h"
#include "jadeDevice.h"
#include "jadeClient.h"
#include "CoreWallet.h"
#include "StringUtils.h"


using namespace bs::hww;

// Helpers to build basic jade cbor request object
static inline QCborMap getRequest(const int id, const QString& method)
{
   QCborMap req;
   req.insert(QCborValue(QLatin1Literal("id")), QString::number(id));
   req.insert(QCborValue(QLatin1Literal("method")), method);
   return req;
}

static inline QCborMap getRequest(const int id, const QString& method
   , const QCborValue& params)
{
   QCborMap req(getRequest(id, method));
   req.insert(QCborValue(QLatin1Literal("params")), params);
   return req;
}


JadeDevice::JadeDevice(const std::shared_ptr<spdlog::logger> &logger
   , bool testNet, DeviceCallbacks* cb, const QSerialPortInfo& endpoint)
   : bs::WorkerPool(1, 1)
   , logger_(logger), testNet_(testNet), cb_(cb), endpoint_(endpoint)
   , handlers_{ std::make_shared<JadeSerialHandler>(logger_, endpoint_)
      , std::make_shared<JadeHttpHandler>(logger_) }
{}

JadeDevice::~JadeDevice() = default;

std::shared_ptr<bs::Worker> JadeDevice::worker(const std::shared_ptr<InData>&)
{
   return std::make_shared<bs::WorkerImpl>(handlers_);
}

void JadeDevice::operationFailed(const std::string& reason)
{
   releaseConnection();
   cb_->operationFailed(key().id, reason);
}

static std::string dump(const QCborMap&);
static std::string dump(const QCborValueRef&);
static std::string dump(const QCborArray& ary)
{
   std::string result = "[";
   for (const auto& it : ary) {
      result += dump(it) + ", ";
   }
   if (result.size() > 3) {
      result.pop_back(), result.pop_back();
   }
   result += "]";
   return result;
}

static std::string dump(const QCborMap& map)
{
   std::string result = "{";
   for (const auto& it : map) {
      result += it.first.toString().toStdString() + "=" + dump(it.second) + ", ";
   }
   if (result.size() > 3) {
      result.pop_back(), result.pop_back();
   }
   result += "}";
   return result;
}

static std::string dump(const QCborValueRef& val)
{
   if (val.isInvalid()) {
      return "<inv>";
   }
   if (val.isMap()) {
      return dump(val.toMap());
   }
   if (val.isArray()) {
      return dump(val.toArray());
   }
   if (val.isString()) {
      return "\"" + val.toString().toStdString() + "\"";
   }
   if (val.isBool()) {
      return val.toBool() ? "true" : "false";
   }
   if (val.isDouble()) {
      return std::to_string(val.toDouble());
   }
   if (val.isInteger()) {
      return std::to_string(val.toInteger());
   }
   if (val.isByteArray()) {
      return bs::toHex(val.toByteArray().toStdString());
   }
   if (val.isDateTime()) {
      return val.toDateTime().toString().toStdString();
   }
   if (val.isUrl()) {
      return val.toUrl().toString().toStdString();
   }
   if (val.isUuid()) {
      return val.toUuid().toString().toStdString();
   }
   if (val.isRegularExpression()) {
      return val.toRegularExpression().pattern().toStdString();
   }
   if (val.isNull()) {
      return "<null>";
   }
   if (val.isUndefined()) {
      return "<undefined>";
   }
   return "<type " + std::to_string(val.type()) + ">";
}

static std::string addrType(AddressEntryType aet)
{
   switch (aet) {
   case AddressEntryType_P2PKH:  return "pkh(k)";
   case AddressEntryType_P2WPKH: return "wpkh(k)";
   case AddressEntryType_P2SH:   return "sh(k)";
   case AddressEntryType_P2WSH:  return "sh(wpkh(k))";
   default: break;
   }
   return "unknown";
}

static QCborArray convertPath(const bs::hd::Path& path)
{
   QCborArray pathArray;
   for (const auto val : path) {
      pathArray.append((quint32)val);
   }
   return pathArray;
}

void JadeDevice::setSupportingTXs(const std::vector<Tx>& txs)
{
   if (!awaitingTXreq_.isValid()) {
      logger_->error("[JadeDevice::setSupportingTXs] awaiting TX request is invalid");
      return;
   }
   if (awaitingTXreq_.armorySigner_.getTxInCount() != txs.size()) {
      logger_->error("[JadeDevice::setSupportingTXs] awaiting TX request inputs"
         " count mismatch: {} vs {}", awaitingTXreq_.armorySigner_.getTxInCount(), txs.size());
      return;
   }
   logger_->debug("[JadeDevice::setSupportingTXs] {}, lock time: {}", txs.size(), awaitingTXreq_.armorySigner_.getLockTime());
   for (const auto& tx : txs) {
      awaitingTXreq_.armorySigner_.addSupportingTx(tx);
   }
   const auto& tx = awaitingTXreq_.armorySigner_.serializeUnsignedTx();
   QCborArray change;
   for (int i = 0; i < awaitingTXreq_.armorySigner_.getTxOutCount() - 1; ++i) {
      change.push_back(nullptr);
   }
   if (awaitingTXreq_.change.address.empty()) {
      change.push_back(nullptr);
   }
   else {
      const auto changePath = bs::hd::Path::fromString(awaitingTXreq_.change.index);
      bs::hd::Path path;
      path.append(bs::hd::Purpose::Native + bs::hd::hardFlag);
      path.append(testNet_ ? bs::hd::CoinType::Bitcoin_test : bs::hd::CoinType::Bitcoin_main);
      path.append(bs::hd::hardFlag);
      path.append(changePath.get(-2));
      path.append(changePath.get(-1));

      change.push_back(QCborMap{ {QLatin1Literal("variant"), QString::fromStdString(addrType(awaitingTXreq_.change.address.getType()))}
         , {QLatin1Literal("path"), convertPath(path)} });
   }
   const QCborMap params = { {QLatin1Literal("network"), network()}, {QLatin1Literal("txn")
      , QByteArray::fromStdString(tx.toBinStr())}, {QLatin1Literal("use_ae_signatures"), false}
      , {QLatin1Literal("num_inputs"), awaitingTXreq_.armorySigner_.getTxInCount()}
      , {QLatin1Literal("change"), change} };
   if (!awaitingTXreq_.change.address.empty()) {
   }
   auto inReq = std::make_shared<JadeSerialIn>();
   inReq->request = getRequest(++seqId_, QLatin1Literal("sign_tx"), params);

   const auto& cbSign = [this, txs](const std::shared_ptr<OutData>& out)
   {
      const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
      if (!data) {
         logger_->error("[JadeDevice::signTX] invalid data");
         cb_->operationFailed(key().id, "Invalid data");
         return;
      }
      if (data->futResponse.wait_for(std::chrono::seconds{ 15 }) != std::future_status::ready) {
         logger_->error("[JadeDevice::signTX] data timeout");
         cb_->operationFailed(key().id, "Device timeout");
         return;
      }
      const auto& msg = data->futResponse.get();
      logger_->debug("[JadeDevice::signTX] response: {}", dump(msg));
      if (msg.contains(QLatin1Literal("error"))) {
         cb_->operationFailed(key().id, msg[QLatin1Literal("error")][QLatin1Literal("message")].toString().toStdString());
         return;
      }
      if (msg[QLatin1Literal("result")].isFalse()) {
         cb_->operationFailed(key().id, "Device refused to sign");
         return;
      }

      auto bw = std::make_shared<BinaryWriter>();
      bw->put_var_int(awaitingTXreq_.armorySigner_.getTxInCount());
      const auto& addSignedInput = [this, bw, txs](uint32_t i, const BinaryData& signedData)
      {
         bw->put_uint32_t(i);
         bw->put_var_int(signedData.getSize());
         bw->put_BinaryData(signedData);

         logger_->debug("[JadeDevice::setSupportingTXs::addSignedInput] {} of {}", i + 1, awaitingTXreq_.armorySigner_.getTxInCount());
         if ((i + 1) >= awaitingTXreq_.armorySigner_.getTxInCount()) {
            cb_->txSigned(key(), bw->getData());
            logger_->debug("[JadeDevice::setSupportingTXs::addSignedInput] done");
         }
      };

      for (uint32_t i = 0; i < awaitingTXreq_.armorySigner_.getTxInCount(); ++i) {
         const auto& spender = awaitingTXreq_.armorySigner_.getSpender(i);
         if (!spender) {
            logger_->warn("[JadeDevice::signTX] no spender at {}", __func__, i);
            continue;
         }
         auto bip32Paths = spender->getBip32Paths();
         if (bip32Paths.size() != 1) {
            logger_->error("[TrezorDevice::handleTxRequest] TXINPUT {} BIP32 paths", bip32Paths.size());
            throw std::logic_error("unexpected pubkey count for spender");
         }
         const auto& path = bip32Paths.begin()->second.getDerivationPathFromSeed();
         QCborArray paramPath;
         for (unsigned i = 0; i < path.size(); i++) {
            //skip first index, it's the wallet root fingerprint
            paramPath.append(path.at(i));
         }
         QCborMap params = { {QLatin1Literal("script"), QByteArray::fromStdString(spender->getOutputScript().toBinStr())}
            , {QLatin1Literal("input_tx"), QByteArray::fromStdString(txs.at(i).serialize().toBinStr())}
            //, {QLatin1Literal("input_tx"), nullptr }
            , {QLatin1Literal("satoshi"), (qint64)spender->getValue()}
            , {QLatin1Literal("is_witness"), true}, {QLatin1Literal("path"), paramPath} };
         auto inInput = std::make_shared<JadeSerialIn>();
         inInput->request = getRequest(++seqId_, QLatin1Literal("tx_input"), params);

         const auto& cbInput = [this, i, addSignedInput](const std::shared_ptr<OutData>& out)
         {
            const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
            if (!data) {
               return;
            }
            if (data->futResponse.wait_for(std::chrono::milliseconds{ 15000 }) != std::future_status::ready) {
               return;                                              //FIXME^
            }
            const auto& msg = data->futResponse.get();
            logger_->debug("[JadeDevice::tx_input] #{} response1: {}", i, dump(msg));
            if (msg.contains(QLatin1Literal("error"))) {
               cb_->operationFailed(key().id, msg[QLatin1Literal("error")][QLatin1Literal("message")].toString().toStdString());
               return;
            }
            const auto binSignedData = BinaryData::fromString(msg[QLatin1Literal("result")].toByteArray().toStdString());
            addSignedInput(i, binSignedData);
         };
         processQueued(inInput, cbInput);
      }
#if 0 //temporarily
      for (uint32_t i = 0; i < awaitingTXreq_.armorySigner_.getTxInCount(); ++i) {
         const auto& cbResponse = [this, i, addSignedInput](const std::shared_ptr<OutData>& out)
         {
            const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
            if (!data) {
               logger_->warn("[JadeDevice::tx_input] invalid data");
               //cb_->operationFailed(key().id, "tx_input invalid data");
               return;
            }
            if (data->futResponse.wait_for(std::chrono::seconds{ 15 }) != std::future_status::ready) {
               logger_->error("[JadeDevice::tx_input] data timeout");
               //cb_->operationFailed(key().id, "tx_input data timeout");
               return;
            }
            const auto& msg = data->futResponse.get();
            logger_->debug("[JadeDevice::tx_input] #{} response2: {}", i, dump(msg));
            if (msg.contains(QLatin1Literal("error"))) {
               cb_->operationFailed(key().id, msg[QLatin1Literal("error")][QLatin1Literal("message")].toString().toStdString());
               return;
            }
            const auto binSignedData = BinaryData::fromString(msg[QLatin1Literal("result")].toByteArray().toStdString());
            addSignedInput(i, binSignedData);
         };
         auto inResponse = std::make_shared<JadeSerialIn>();
         processQueued(inResponse, cbResponse);
      }
#endif
   };
   processQueued(inReq, cbSign);
}

void JadeDevice::releaseConnection()
{
   WorkerPool::cancel();
}

std::string bs::hww::JadeDevice::idFromSerial(const QSerialPortInfo& serial)
{
   return serial.hasProductIdentifier() ? std::to_string(serial.productIdentifier())
      : serial.portName().toStdString();
}

DeviceKey JadeDevice::key() const
{
   std::string status;
   if (walletId_.empty()) {
      if (!xpubRoot_.empty()) {
         try {
            /*const auto& seed = bs::core::wallet::Seed::fromXpub(xpubRoot_
               , testNet_ ? NetworkType::TestNet : NetworkType::MainNet);*/
            walletId_ = bs::core::wallet::computeID(xpubRoot_).toBinStr();
         }
         catch (const std::exception& e) {
            logger_->error("[{}] failed to get walletId from {}: {}", __func__, xpubRoot_.toBinStr(), e.what());
         }
      }
      else {
         status = "not inited";
      }
   }
   return { "Jade @" + endpoint_.portName().toStdString()
      , idFromSerial(endpoint_)
      , endpoint_.hasVendorIdentifier() ? std::to_string(endpoint_.vendorIdentifier()) : endpoint_.manufacturer().toStdString()
      , walletId_, type() };
   return {};
}

DeviceType JadeDevice::type() const
{
   return DeviceType::HWJade;
}

static uint32_t epoch()
{
   return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void JadeDevice::init()
{
   if (inited()) {
      logger_->debug("[JadeDevice::init] already inited");
      cb_->publicKeyReady(key());
      return;
   }
   logger_->debug("[JadeDevice::init] start");
   auto in = std::make_shared<JadeSerialIn>();
   in->request = getRequest(++seqId_, QLatin1Literal("get_version_info"));

   const auto& cbXPub = [this](const std::shared_ptr<OutData>& out)
   {
      const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
      if (!data) {
         logger_->error("[JadeDevice::init::xpub] invalid data");
         cb_->publicKeyReady(key());
         return;
      }
      if (data->futResponse.wait_for(std::chrono::milliseconds{ 1500 }) != std::future_status::ready) {
         logger_->error("[JadeDevice::init::xpub] data timeout");
         cb_->publicKeyReady(key());
         return;
      }
      const auto& msg = data->futResponse.get();
      logger_->debug("[JadeDevice::init] xpub response: {}", dump(msg));

      xpubRoot_ = BinaryData::fromString(msg[QLatin1Literal("result")].toString().toStdString());
      cb_->publicKeyReady(key());
   };

   const auto& cbVersion = [this, cbXPub](const std::shared_ptr<OutData>& out)
   {
      const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
      if (!data) {
         logger_->error("[JadeDevice::init::version] invalid data");
         cb_->publicKeyReady(key());
         return;
      }
      if (data->futResponse.wait_for(std::chrono::milliseconds{ 1500 }) != std::future_status::ready) {
         logger_->error("[JadeDevice::init::version] data timeout");
         cb_->publicKeyReady(key());
         return;
      }
      const auto& msg = data->futResponse.get();
      logger_->debug("[JadeDevice::init] version response: {}", dump(msg));

      const auto& netType = msg[QLatin1Literal("result")][QLatin1Literal("JADE_NETWORKS")].toString().toStdString();
      if ((netType != "ALL") && (netType != (testNet_ ? "TEST" : "MAIN"))) {
         logger_->error("[JadeDevice::init] network type mismatch: {}", netType);
         cb_->publicKeyReady(key());
         return;
      }

      auto inXpub = std::make_shared<JadeSerialIn>();
      bs::hd::Path path;
      path.append(bs::hd::hardFlag);
      path.append(testNet_ ? bs::hd::CoinType::Bitcoin_test : bs::hd::CoinType::Bitcoin_main);
      const QCborMap params = { {QLatin1Literal("network"), network()}, {QLatin1Literal("path"), convertPath(path)}};
      inXpub->request = getRequest(++seqId_, QLatin1Literal("get_xpub"), params);

      if (msg[QLatin1Literal("result")][QLatin1Literal("JADE_STATE")].toString().toStdString() == "LOCKED") {
         if (cb_) {
            cb_->requestHWPass(key(), true);
         }
         const QCborMap authParams{ {QLatin1Literal("network"), network()}, {QLatin1Literal("epoch"), epoch()} };
         auto inAuth = std::make_shared<JadeSerialIn>();
         inAuth->request = getRequest(++seqId_, QLatin1Literal("auth_user"), authParams);
         const auto& cbAuth = [this, cbXPub, inXpub](const std::shared_ptr<OutData>& out)
         {
            const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
            if (!data) {
               logger_->error("[JadeDevice::init::auth] invalid data");
               cb_->publicKeyReady(key());
               return;
            }
            if (data->futResponse.wait_for(std::chrono::seconds{ 30 }) != std::future_status::ready) {
               logger_->error("[JadeDevice::init::auth] data timeout");
               cb_->publicKeyReady(key());
               return;
            }
            const auto& msg = data->futResponse.get();
            logger_->debug("[JadeDevice::init] auth response: {}", dump(msg));

            if (msg[QLatin1Literal("result")].isBool()) {
               if (msg[QLatin1Literal("result")].isTrue()) {
                  processQueued(inXpub, cbXPub);
               }
               else {
                  logger_->error("[JadeDevice::init::auth] failed");
                  cb_->publicKeyReady(key());
                  return;
               }
            }
            else {   // forward request to PIN server
               const auto& httpParams = msg[QLatin1Literal("result")][QLatin1Literal("http_request")][QLatin1Literal("params")];
               auto inHttp = std::make_shared<JadeHttpIn>();
               inHttp->url = httpParams[QLatin1Literal("urls")].toArray().at(0).toString().toStdString();
               inHttp->data = httpParams[QLatin1Literal("data")].toString().toStdString();
               const auto onReply = msg[QLatin1Literal("result")][QLatin1Literal("http_request")][QLatin1Literal("on-reply")].toString();

               const auto& cbHttp = [this, onReply, cbXPub, inXpub](const std::shared_ptr<OutData>& out)
               {
                  const auto& data = std::static_pointer_cast<JadeHttpOut>(out);
                  if (!data || data->response.empty()) {
                     logger_->error("[JadeDevice::init::http] invalid data");
                     cb_->publicKeyReady(key());
                     return;
                  }
                  QJsonDocument jsonDoc = QJsonDocument::fromJson(QByteArray::fromStdString(data->response));
                  const auto& params = QCborMap::fromJsonObject(jsonDoc.object());
                  auto inResp = std::make_shared<JadeSerialIn>();
                  inResp->request = getRequest(++seqId_, onReply, params);
                  const auto& cbHandshake = [this, cbXPub, inXpub](const std::shared_ptr<OutData>& out)
                  {
                     const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
                     if (!data) {
                        logger_->error("[JadeDevice::init::handshake] invalid data");
                        cb_->publicKeyReady(key());
                        return;
                     }
                     if (data->futResponse.wait_for(std::chrono::seconds{ 23 }) != std::future_status::ready) {
                        logger_->error("[JadeDevice::init::handshake] data timeout");
                        cb_->publicKeyReady(key());
                        return;
                     }
                     const auto& msg = data->futResponse.get();
                     logger_->debug("[JadeDevice::init] handshake response: {}", dump(msg));

                     const auto& httpParams = msg[QLatin1Literal("result")][QLatin1Literal("http_request")][QLatin1Literal("params")];
                     auto inHttp = std::make_shared<JadeHttpIn>();
                     inHttp->url = httpParams[QLatin1Literal("urls")].toArray().at(0).toString().toStdString();
                     const auto& jsonData = httpParams[QLatin1Literal("data")].toJsonValue().toObject();
                     inHttp->data = QJsonDocument(jsonData).toJson().toStdString();
                     const auto onReply = msg[QLatin1Literal("result")][QLatin1Literal("http_request")][QLatin1Literal("on-reply")].toString();

                     const auto& cbReply = [this, onReply, cbXPub, inXpub](const std::shared_ptr<OutData>& out)
                     {
                        const auto& data = std::static_pointer_cast<JadeHttpOut>(out);
                        if (!data || data->response.empty()) {
                           logger_->error("[JadeDevice::init::reply] invalid data");
                           cb_->publicKeyReady(key());
                           return;
                        }
                        QJsonDocument jsonDoc = QJsonDocument::fromJson(QByteArray::fromStdString(data->response));
                        const auto& params = QCborMap::fromJsonObject(jsonDoc.object());
                        auto inComplete = std::make_shared<JadeSerialIn>();
                        inComplete->request = getRequest(++seqId_, onReply, params);
                        const auto& cbComplete = [this, cbXPub, inXpub](const std::shared_ptr<OutData>& out)
                        {
                           const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
                           if (!data) {
                              logger_->error("[JadeDevice::init::complete] invalid data");
                              cb_->publicKeyReady(key());
                              return;
                           }
                           if (data->futResponse.wait_for(std::chrono::seconds{ 3 }) != std::future_status::ready) {
                              logger_->error("[JadeDevice::init::complete] data timeout");
                              cb_->publicKeyReady(key());
                              return;
                           }
                           const auto& msg = data->futResponse.get();
                           logger_->debug("[JadeDevice::init] complete response: {}", dump(msg));
                           if (msg[QLatin1Literal("result")].isBool() && msg[QLatin1Literal("result")].isTrue()) {
                              processQueued(inXpub, cbXPub);
                           }
                           else {
                              logger_->error("[JadeDevice::init::complete] handshake failed");
                              cb_->publicKeyReady(key());
                           }
                        };
                        processQueued(inComplete, cbComplete);
                     };
                     processQueued(inHttp, cbReply);
                  };
                  processQueued(inResp, cbHandshake);
               };
               processQueued(inHttp, cbHttp);
            }
         };
         processQueued(inAuth, cbAuth);
      }
      else {
         processQueued(inXpub, cbXPub);
      }
   };
   processQueued(in, cbVersion);
}

void JadeDevice::getPublicKeys()
{
   awaitingWalletInfo_ = {};
   awaitingWalletInfo_.type = bs::wallet::HardwareEncKey::WalletType::Jade;
   awaitingWalletInfo_.xpubRoot = xpubRoot_.toBinStr();
   awaitingWalletInfo_.label = key().label;
   awaitingWalletInfo_.deviceId = key().id;
   awaitingWalletInfo_.vendor = key().vendor;

   const auto& requestXPub = [this](bs::hd::Purpose purp)->std::pair<std::shared_ptr<JadeSerialIn>, bs::WorkerPool::callback>
   {
      auto inXpub = std::make_shared<JadeSerialIn>();
      bs::hd::Path path;
      path.append(purp + bs::hd::hardFlag);
      path.append(testNet_ ? bs::hd::CoinType::Bitcoin_test : bs::hd::CoinType::Bitcoin_main);
      path.append(bs::hd::hardFlag);
      const QCborMap params = { {QLatin1Literal("network"), network()}, {QLatin1Literal("path"), convertPath(path)} };
      inXpub->request = getRequest(++seqId_, QLatin1Literal("get_xpub"), params);

      return {inXpub , [this, purp](const std::shared_ptr<OutData>& out)
         {
            const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
            if (!data) {
               logger_->error("[JadeDevice::getPublicKeys::xpub] invalid data");
               cb_->publicKeyReady(key());
               return;
            }
            if (data->futResponse.wait_for(std::chrono::milliseconds{ 1500 }) != std::future_status::ready) {
               logger_->error("[JadeDevice::getPublicKeys::xpub] data timeout");
               cb_->publicKeyReady(key());
               return;
            }
            const auto& msg = data->futResponse.get();
            logger_->debug("[JadeDevice::getPublicKeys] xpub response: {}", dump(msg));

            if (purp == bs::hd::Purpose::Native) {
               awaitingWalletInfo_.xpubNativeSegwit = msg[QLatin1Literal("result")].toString().toStdString();
            }
            else if (purp == bs::hd::Purpose::Nested) {
               awaitingWalletInfo_.xpubNestedSegwit = msg[QLatin1Literal("result")].toString().toStdString();
            }
            else {
               logger_->error("[JadeDevice::getPublicKeys::xpub] unsupported wallet type {}", purp);
            }
            if (!awaitingWalletInfo_.xpubNativeSegwit.empty() && !awaitingWalletInfo_.xpubNestedSegwit.empty()) {
               cb_->walletInfoReady(key(), awaitingWalletInfo_);
            }
         } };
   };
   const auto& inXpubNative = requestXPub(bs::hd::Purpose::Native);
   processQueued(inXpubNative.first, inXpubNative.second);
   const auto& inXpubNested = requestXPub(bs::hd::Purpose::Nested);
   processQueued(inXpubNested.first, inXpubNested.second);
}

void JadeDevice::signTX(const bs::core::wallet::TXSignRequest &reqTX)
{
   logger_->debug("[JadeDevice::signTX]");
   std::vector<BinaryData> txHashes;
   for (uint32_t i = 0; i < reqTX.armorySigner_.getTxInCount(); ++i) {
      const auto& spender = reqTX.armorySigner_.getSpender(i);
      if (!spender) {
         logger_->warn("[JadeDevice::signTX] no spender at {}", i);
         continue;
      }
      txHashes.push_back(spender->getUtxo().getTxHash());

      auto bip32Paths = spender->getBip32Paths();
      if (bip32Paths.size() != 1) {
         logger_->error("[TrezorDevice::signTX] {} BIP32 paths", bip32Paths.size());
         continue;
      }
      const auto& path = bip32Paths.begin()->second.getDerivationPathFromSeed();
      const QCborMap params = { {QLatin1Literal("network"), network()}, {QLatin1Literal("path"), convertPath(path)} };
      auto inXpub = std::make_shared<JadeSerialIn>();
      inXpub->request = getRequest(++seqId_, QLatin1Literal("get_xpub"), params);

      const auto& cbXPub = [this](const std::shared_ptr<OutData>& out)
      {
         const auto& data = std::static_pointer_cast<JadeSerialOut>(out);
         if (!data) {
            logger_->error("[JadeDevice::signTX] invalid data");
            cb_->publicKeyReady(key());
            return;
         }
         if (data->futResponse.wait_for(std::chrono::milliseconds{ 1500 }) != std::future_status::ready) {
            logger_->error("[JadeDevice::signTX] data timeout");
            cb_->publicKeyReady(key());
            return;
         }
         const auto& msg = data->futResponse.get();
         logger_->debug("[JadeDevice::init] xpub response: {}", dump(msg));
      };
      processQueued(inXpub, cbXPub);
   }
   logger_->debug("[JadeDevice::signTX] {} prevOuts requested", txHashes.size());
   cb_->needSupportingTXs(key(), txHashes);
   awaitingTXreq_ = reqTX;
}

void JadeDevice::retrieveXPubRoot()
{
   logger_->debug("[JadeDevice::retrieveXPubRoot]");
}

void JadeDevice::reset()
{
   awaitingTXreq_ = {};
   awaitingWalletInfo_ = {};
}

QString bs::hww::JadeDevice::network() const
{
   return testNet_ ? QLatin1Literal("testnet") : QLatin1Literal("mainnet");
}


JadeSerialHandler::JadeSerialHandler(const std::shared_ptr<spdlog::logger>& logger
   , const QSerialPortInfo& spi)
   : QObject(nullptr), logger_(logger), serial_(new QSerialPort(spi, this))
{
   serial_->setBaudRate(QSerialPort::Baud115200);
   serial_->setDataBits(QSerialPort::Data8);
   serial_->setParity(QSerialPort::NoParity);
   serial_->setStopBits(QSerialPort::OneStop);

   if (!Connect()) {
      throw std::runtime_error("failed to open port " + spi.portName().toStdString());
   }
}

JadeSerialHandler::~JadeSerialHandler()
{
   Disconnect();
}

std::shared_ptr<JadeSerialOut> JadeSerialHandler::processData(const std::shared_ptr<JadeSerialIn>& in)
{
   if (!serial_->isOpen()) {
      return nullptr;
   }
   auto out = std::make_shared<JadeSerialOut>();
   if (in->needResponse) {
      auto prom = std::make_shared<std::promise<QCborMap>>();
      out->futResponse = prom->get_future();
      requests_.push_back(prom);
   }
   logger_->debug("[JadeSerialHandler] sending: {}", dump(in->request));
   if (!in->request.empty()) {
      const auto bytes = in->request.toCborValue().toCbor();
      write(bytes);
   }
   return out;
}

bool bs::hww::JadeSerialHandler::Connect()
{
   if (serial_->isOpen()) {
      return true;   // already connected
   }
   if (serial_->open(QIODevice::ReadWrite)) {
      // Connect 'data received' slot
      connect(serial_, &QSerialPort::readyRead, this
         , &JadeSerialHandler::onSerialDataReady);
      return true;
   }
   Disconnect();
   return false;
}

void bs::hww::JadeSerialHandler::Disconnect()
{
   disconnect(serial_, nullptr, this, nullptr);
   if (serial_->isOpen()) {
      serial_->close();
   }
}

int bs::hww::JadeSerialHandler::write(const QByteArray& data)
{
   assert(serial_);
   assert(serial_->isOpen());

   int written = 0;
   while (written != data.length()) {
      const auto wrote = serial_->write(data.data() + written, qMin(256, data.length() - written));
      if (wrote < 0) {
         logger_->debug("[{}] write error: {}", __func__, wrote);
         Disconnect();
         return written;
      }
      else {
         serial_->waitForBytesWritten(100);
         written += wrote;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
   }
   logger_->debug("[{}] {} bytes (of {}) written", __func__, written, data.size());
   return written;
}

void bs::hww::JadeSerialHandler::parsePortion(const QByteArray& data)
{
   try {
      // Collect data
      unparsed_.append(data);

      // Try to parse cbor objects from byte buffer until it has no more complete objects
      for (bool readNextObj = true; readNextObj; /*nothing - set in loop*/) {
         QCborParserError err;
         const QCborValue cbor = QCborValue::fromCbor(unparsed_, &err);
         //logger_->debug("[{}] read type {}, result={}", __func__, cbor.type()
         //   , err.error.toString().toStdString());
         readNextObj = false;  // In most cases we don't read another object

         if (err.error == QCborError::NoError && cbor.isMap()) {
            const QCborMap msg = cbor.toMap();
            if (msg.contains(QCborValue(QLatin1Literal("log")))) {
               // Print Jade log line immediately
               logger_->info("[{}] {}", __func__, msg[QLatin1Literal("log")]
                  .toByteArray().toStdString());
            }
            else {
               //logger_->debug("[{}] ready: {}", __func__, dump(msg));
               if (requests_.empty()) {
                  logger_->error("[{}] unexpected response", __func__);
               }
               else {
                  requests_.front()->set_value(msg);
                  requests_.pop_front();
               }
            }

            // Remove read object from m_data buffer
            if (err.offset == unparsed_.length()) {
               unparsed_.clear();
            }
            else {
               // We successfully read an object and there are still bytes left in the buffer - this
               // is the one case where we loop and read again - make sure to preserve the remaining bytes.
               const int remainder = static_cast<int>(unparsed_.length() - err.offset);
               unparsed_ = unparsed_.right(remainder);
               readNextObj = true;
               logger_->debug("[{}] {} more data after the message", __func__, remainder);
            }
         }
         else if (err.error == QCborError::EndOfFile) {
            // partial object - stop trying to read objects for now, await more data
            if (unparsed_.length() > 0) {
               logger_->debug("[{}] CBOR incomplete ({} bytes present) - awaiting more data"
                  , __func__, unparsed_.size());
            }
         }
         else {
            // Unexpected parse error
            logger_->warn("[{}] unexpected type {} and/or error: {}", __func__
               , (int)cbor.type(), err.error.toString().toStdString());
            Disconnect();
         }
      }
   }
   catch (const std::exception& e) {
      logger_->error("[{}] exception: {}", __func__, e.what());
      Disconnect();
   }
   catch (...) {
      logger_->error("[{}] exception", __func__);
      Disconnect();
   }
}

void JadeSerialHandler::onSerialDataReady()
{
   assert(serial_);
   const auto& data = serial_->readAll();
   logger_->debug("[{}] {} bytes", __func__, data.size());

   if (requests_.empty()) {
      logger_->error("[{}] dropped {} bytes of serial data", __func__, data.size());
      return;
   }
   parsePortion(data);
}

static size_t writeToString(void* ptr, size_t size, size_t count, std::string* stream)
{
   const size_t resSize = size * count;
   stream->append((char*)ptr, resSize);
   return resSize;
}

JadeHttpHandler::JadeHttpHandler(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
{
   curl_ = curl_easy_init();
   curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, writeToString);
   curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, 0L);
   curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, 0L);
   curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
   curl_easy_setopt(curl_, CURLOPT_POST, 1);

   curlHeaders_ = curl_slist_append(curlHeaders_, "Content-Type: application/json");
   curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, curlHeaders_);
}

JadeHttpHandler::~JadeHttpHandler()
{
   curl_slist_free_all(curlHeaders_);
   curl_easy_cleanup(curl_);
}

std::shared_ptr<JadeHttpOut> JadeHttpHandler::processData(const std::shared_ptr<JadeHttpIn>& in)
{
   auto result = std::make_shared<JadeHttpOut>();
   if (!curl_) {
      return result;
   }
   logger_->debug("[JadeHttpHandler::processData] request: {} {}", in->url, in->data);
   curl_easy_setopt(curl_, CURLOPT_URL, in->url.c_str());
   curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, in->data.data());
   std::string response;
   curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

   const auto res = curl_easy_perform(curl_);
   if (res != CURLE_OK) {
      logger_->error("[JadeHttpHandler::processData] failed to post to {}: {}", in->url, res);
      return result;
   }
   result->response = response;
   logger_->debug("[JadeHttpHandler::processData] response: {}", result->response);
   return result;
}
