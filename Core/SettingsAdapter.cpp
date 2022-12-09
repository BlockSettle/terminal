/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SettingsAdapter.h"
#include <QFile>
#include <QRect>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "ArmoryServersProvider.h"
#include "BootstrapDataManager.h"
#include "Settings/SignersProvider.h"
#include "LogManager.h"
#include "PubKeyLoader.h"
#include "WsConnection.h"

#include "common.pb.h"
#include "terminal.pb.h"

using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace bs::message;


SettingsAdapter::SettingsAdapter(const std::shared_ptr<ApplicationSettings> &settings
   , const QStringList &appArgs)
   : appSettings_(settings)
   , user_(std::make_shared<UserTerminal>(TerminalUsers::Settings))
{
   if (!appSettings_->LoadApplicationSettings(appArgs)) {
      std::cerr << "failed to load settings: " << appSettings_->ErrorText().toStdString() << "\n";
      throw std::runtime_error("failed to load settings: " + appSettings_->ErrorText().toStdString());
   }
   logMgr_ = std::make_shared<bs::LogManager>();
   logMgr_->add(appSettings_->GetLogsConfig());
   logger_ = logMgr_->logger();

   if (!appSettings_->get<bool>(ApplicationSettings::initialized)) {
      appSettings_->SetDefaultSettings(true);
   }
   appSettings_->selectNetwork();
   logger_->debug("Settings loaded from {}", appSettings_->GetSettingsPath().toStdString());

   bootstrapDataManager_ = std::make_shared<BootstrapDataManager>(logMgr_->logger(), appSettings_);
   if (bootstrapDataManager_->hasLocalFile()) {
      bootstrapDataManager_->loadFromLocalFile();
   } else {
      // load from resources
      const QString filePathInResources = appSettings_->bootstrapResourceFileName();

      QFile file;
      file.setFileName(filePathInResources);
      if (file.open(QIODevice::ReadOnly)) {
         const std::string bootstrapData = file.readAll().toStdString();
         if (!bootstrapDataManager_->setReceivedData(bootstrapData)) {
            logger_->error("[SettingsAdapter] invalid bootstrap data: {}"
               , filePathInResources.toStdString());
         }
      } else {
         logger_->error("[SettingsAdapter] failed to locate bootstrap file in resources: {}"
            , filePathInResources.toStdString());
      }
   }

   armoryServersProvider_ = std::make_shared<ArmoryServersProvider>(settings, bootstrapDataManager_);
   signersProvider_ = std::make_shared<SignersProvider>(appSettings_);
}

ProcessingResult SettingsAdapter::process(const bs::message::Envelope &env)
{
   if (env.receiver->value<TerminalUsers>() == TerminalUsers::Settings) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings msg #{}", __func__, env.foreignId());
         return ProcessingResult::Error;
      }
      switch (msg.data_case()) {
      case SettingsMessage::kGetRequest:
         return processGetRequest(env, msg.get_request());
      case SettingsMessage::kPutRequest:
         return processPutRequest(msg.put_request());
      case SettingsMessage::kArmoryServer:
         return processArmoryServer(msg.armory_server());
      case SettingsMessage::kSetArmoryServer:
         return processSetArmoryServer(env, msg.set_armory_server());
      case SettingsMessage::kArmoryServersGet:
         return processGetArmoryServers(env);
      case SettingsMessage::kAddArmoryServer:
         return processAddArmoryServer(env, msg.add_armory_server());
      case SettingsMessage::kDelArmoryServer:
         return processDelArmoryServer(env, msg.del_armory_server());
      case SettingsMessage::kUpdArmoryServer:
         return processUpdArmoryServer(env, msg.upd_armory_server());
      case SettingsMessage::kSignerRequest:
         return processSignerSettings(env);
      case SettingsMessage::kSignerSetKey:
         return processSignerSetKey(msg.signer_set_key());
      case SettingsMessage::kSignerReset:
         return processSignerReset();
      case SettingsMessage::kSignerServersGet:
         return processGetSigners(env);
      case SettingsMessage::kSetSignerServer:
         return processSetSigner(env, msg.set_signer_server());
      case SettingsMessage::kAddSignerServer:
         return processAddSigner(env, msg.add_signer_server());
      case SettingsMessage::kDelSignerServer:
         return processDelSigner(env, msg.del_signer_server());
      case SettingsMessage::kStateGet:
         return processGetState(env);
      case SettingsMessage::kReset:
         return processReset(env, msg.reset());
      case SettingsMessage::kResetToState:
         return processResetToState(env, msg.reset_to_state());
      case SettingsMessage::kLoadBootstrap:
         return processBootstrap(env, msg.load_bootstrap());
      case SettingsMessage::kGetBootstrap:
         return processBootstrap(env, {});
      case SettingsMessage::kApiPrivkey:
         return processApiPrivKey(env);
      case SettingsMessage::kApiClientKeys:
         return processApiClientsList(env);
      default:
         logger_->warn("[SettingsAdapter::process] unknown data case: {}"
            , msg.data_case());
         break;
      }
   }
   return ProcessingResult::Ignored;
}

bool SettingsAdapter::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->value<TerminalUsers>() == TerminalUsers::Blockchain) {
      ArmoryMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse armory msg #{}", __func__, env.foreignId());
         return false;
      }
      if (msg.data_case() == ArmoryMessage::kSettingsRequest) {
         ArmoryMessage msgReply;
         auto msgResp = msgReply.mutable_settings_response();
         const auto& armorySettings = armoryServersProvider_->getArmorySettings();
         armoryServersProvider_->setConnectedArmorySettings(armorySettings);
         msgResp->set_socket_type(armorySettings.socketType);
         msgResp->set_network_type((int)armorySettings.netType);
         msgResp->set_host(armorySettings.armoryDBIp.toStdString());
         msgResp->set_port(std::to_string(armorySettings.armoryDBPort));
         msgResp->set_bip15x_key(armorySettings.armoryDBKey.toStdString());
         msgResp->set_run_locally(armorySettings.runLocally);
         msgResp->set_data_dir(armorySettings.dataDir.toStdString());
         msgResp->set_executable_path(armorySettings.armoryExecutablePath.toStdString());
         msgResp->set_bitcoin_dir(armorySettings.bitcoinBlocksDir.toStdString());
         msgResp->set_db_dir(armorySettings.dbDir.toStdString());
         msgResp->set_cache_file_name(appSettings_->get<std::string>(ApplicationSettings::txCacheFileName));
         pushResponse(user_, env, msgReply.SerializeAsString());
         return true;
      }
   }
   return false;
}

ProcessingResult SettingsAdapter::processRemoteSettings(uint64_t msgId)
{
   const auto &itReq = remoteSetReqs_.find(msgId);
   if (itReq == remoteSetReqs_.end()) {
      logger_->error("[{}] failed to find remote settings request #{}", __func__
         , msgId);
      return ProcessingResult::Error;
   }
   return ProcessingResult::Ignored;
}

ProcessingResult SettingsAdapter::processGetState(const bs::message::Envelope& env)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_state();
   for (const auto& st : appSettings_->getState()) {
      auto setResp = msgResp->add_responses();
      auto setReq = setResp->mutable_request();
      setReq->set_source(SettingSource_Local);
      setReq->set_index(static_cast<SettingIndex>(st.first));
      setFromQVariant(st.second, setReq, setResp);
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult SettingsAdapter::processReset(const bs::message::Envelope& env
   , const SettingsMessage_SettingsRequest& request)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_state();
   for (const auto& req : request.requests()) {
      auto setResp = msgResp->add_responses();
      auto setReq = setResp->mutable_request();
      setReq->set_source(req.source());
      setReq->set_index(req.index());
      const auto& setting = static_cast<ApplicationSettings::Setting>(req.index());
      appSettings_->reset(setting);
      const auto& value = appSettings_->get(setting);
      setFromQVariant(value, setReq, setResp);
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult SettingsAdapter::processResetToState(const bs::message::Envelope& env
   , const SettingsMessage_SettingsResponse& request)
{
   for (const auto& req : request.responses()) {
      const auto& value = fromResponse(req);
      const auto& setting = static_cast<ApplicationSettings::Setting>(req.request().index());
      appSettings_->set(setting, value);
   }
   SettingsMessage msg;
   *msg.mutable_state() = request;
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult SettingsAdapter::processBootstrap(const bs::message::Envelope &env
   , const std::string& bsData)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_bootstrap();
   bool result = true;
   if (!bsData.empty()) {
      result = bootstrapDataManager_->setReceivedData(bsData);
   }
   msgResp->set_loaded(result);
   if (result) {
      for (const auto& validationAddr : bootstrapDataManager_->GetAuthValidationList()) {
         msgResp->add_auth_validations(validationAddr);
      }
      for (const auto& ccDef : bootstrapDataManager_->GetCCDefinitions()) {
         auto msgCcDef = msgResp->add_cc_definitions();
         msgCcDef->set_security_id(ccDef.securityId);
         msgCcDef->set_product(ccDef.product);
         msgCcDef->set_genesis_address(ccDef.genesisAddr.display());
         msgCcDef->set_lot_size(ccDef.nbSatoshis);
      }
   }
   else {
      logger_->error("[{}] failed to set bootstrap data", __func__);
   }
   pushResponse(user_, bsData.empty() ? env.sender : nullptr
      , msg.SerializeAsString(), bsData.empty() ? env.foreignId() : 0);
   return ProcessingResult::Success;
}

static bs::network::ws::PrivateKey readOrCreatePrivateKey(const std::string& filename)
{
   bs::network::ws::PrivateKey result;
   std::ifstream privKeyReader(filename, std::ios::binary);
   if (privKeyReader.is_open()) {
      std::string str;
      str.assign(std::istreambuf_iterator<char>(privKeyReader)
         , std::istreambuf_iterator<char>());
      result.reserve(str.size());
      std::for_each(str.cbegin(), str.cend(), [&result](char c) {
         result.push_back(c);
      });
   }
   if (result.empty()) {
      result = bs::network::ws::generatePrivKey();
      std::ofstream privKeyWriter(filename, std::ios::out | std::ios::binary);
      privKeyWriter.write((char*)&result[0], result.size());
      const auto& pubKey = bs::network::ws::publicKey(result);
      std::ofstream pubKeyWriter(filename + ".pub", std::ios::out | std::ios::binary);
      pubKeyWriter << pubKey;
   }
   return result;
}

ProcessingResult SettingsAdapter::processApiPrivKey(const bs::message::Envelope& env)
{  //FIXME: should be re-implemented to avoid storing plain private key in a file on disk
   const auto &apiKeyFN = appSettings_->AppendToWritableDir(
      QString::fromStdString("apiPrivKey")).toStdString();
   const auto &apiPrivKey = readOrCreatePrivateKey(apiKeyFN);
   SettingsMessage msg;
   SecureBinaryData privKey(apiPrivKey.data(), apiPrivKey.size());
   msg.set_api_privkey(privKey.toBinStr());
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

static std::set<std::string> readClientKeys(const std::string& filename)
{
   std::ifstream in(filename);
   if (!in.is_open()) {
      return {};
   }
   std::set<std::string> result;
   std::string str;
   while (std::getline(in, str)) {
      if (str.empty()) {
         continue;
      }
      try {
         const auto& pubKey = BinaryData::CreateFromHex(str).toBinStr();
         result.insert(pubKey);
      } catch (const std::exception&) {}   // ignore invalid keys for now
   }
   return result;
}

ProcessingResult SettingsAdapter::processApiClientsList(const bs::message::Envelope& env)
{
   const auto& apiKeysFN = appSettings_->AppendToWritableDir(
      QString::fromStdString("apiClientPubKeys")).toStdString();
   const auto& clientKeys = readClientKeys(apiKeysFN);
   if (clientKeys.empty()) {
      logger_->debug("[{}] no API client keys found", __func__);
   }
   SettingsMessage msg;
   const auto msgResp = msg.mutable_api_client_keys();
   for (const auto& clientKey : clientKeys) {
      msgResp->add_pub_keys(clientKey);
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

std::string SettingsAdapter::guiMode() const
{
   if (!appSettings_) {
      return {};
   }
   return appSettings_->guiMode().toStdString();
}

ProcessingResult SettingsAdapter::processGetRequest(const bs::message::Envelope &env
   , const SettingsMessage_SettingsRequest &request)
{
   unsigned int nbFetched = 0;
   SettingsMessage msg;
   auto msgResp = msg.mutable_get_response();
   for (const auto &req : request.requests()) {
      auto resp = msgResp->add_responses();
      resp->set_allocated_request(new SettingRequest(req));
      if (req.source() == SettingSource_Local) {
         switch (req.index()) {
         case SetIdx_BlockSettleSignAddress:
            resp->set_s(appSettings_->GetBlocksettleSignAddress());
            resp->mutable_request()->set_type(SettingType_String);
            break;
         default: {
            const auto setting = static_cast<ApplicationSettings::Setting>(req.index());
            switch (req.type()) {
            case SettingType_Unknown: {
               const auto& value = appSettings_->get(setting);
               switch (value.type()) {
               case QVariant::Type::String:
                  resp->set_s(value.toString().toStdString());
                  resp->mutable_request()->set_type(SettingType_String);
                  break;
               default: // normally string is used for all unknown values
                  logger_->warn("[{}] {}: unhandled QVariant type: {}", __func__
                     , (int)setting, (int)value.type());
                  break;
               }
            }
               break;
            case SettingType_String:
               resp->set_s(appSettings_->get<std::string>(setting));
               break;
            case SettingType_Int:
               resp->set_i(appSettings_->get<int>(setting));
               break;
            case SettingType_UInt:
               resp->set_ui(appSettings_->get<unsigned int>(setting));
               break;
            case SettingType_UInt64:
               resp->set_ui64(appSettings_->get<uint64_t>(setting));
               break;
            case SettingType_Bool:
               resp->set_b(appSettings_->get<bool>(setting));
               break;
            case SettingType_Float:
               resp->set_f(appSettings_->get<double>(setting));
               break;
            case SettingType_Rect: {
               const auto& rect = appSettings_->get<QRect>(setting);
               auto r = resp->mutable_rect();
               r->set_left(rect.left());
               r->set_top(rect.top());
               r->set_width(rect.width());
               r->set_height(rect.height());
            }
                                 break;
            case SettingType_Strings: {
               const auto& strings = appSettings_->get<QStringList>(setting);
               auto ss = resp->mutable_strings();
               for (const auto& string : strings) {
                  ss->add_strings(string.toStdString());
               }
            }
                                    break;
            case SettingType_StrMap: {
               const auto& map = appSettings_->get<QVariantMap>(setting);
               auto keyVals = resp->mutable_key_vals();
               for (auto it = map.begin(); it != map.end(); it++) {
                  auto sm = keyVals->add_key_vals();
                  sm->set_key(it.key().toStdString());
                  sm->set_value(it.value().toString().toStdString());
               }
            }
                                   break;
            default:
               logger_->error("[{}] unknown setting type: {}", __func__, (int)req.type());
               nbFetched--;
               break;
            }
         }
                break;
         }
         nbFetched++;
      }
      else {
         logger_->error("[{}] unknown settings source: {}", __func__, req.source());
      }
   }
   if (nbFetched > 0) {
      pushResponse(user_, env, msg.SerializeAsString());
      return ProcessingResult::Success;
   }
   return ProcessingResult::Ignored;
}

ProcessingResult SettingsAdapter::processPutRequest(const SettingsMessage_SettingsResponse &request)
{
   unsigned int nbUpdates = 0;
   for (const auto &req : request.responses()) {
      if (req.request().source() == SettingSource_Local) {
         const auto setting = static_cast<ApplicationSettings::Setting>(req.request().index());
         switch (req.request().type()) {
         case SettingType_String:
            appSettings_->set(setting, QString::fromStdString(req.s()));
            break;
         case SettingType_Int:
            appSettings_->set(setting, req.i());
            break;
         case SettingType_UInt:
            appSettings_->set(setting, req.ui());
            break;
         case SettingType_UInt64:
            appSettings_->set(setting, quint64(req.ui64()));
            break;
         case SettingType_Bool:
            appSettings_->set(setting, req.b());
            break;
         case SettingType_Float:
            appSettings_->set(setting, req.f());
            break;
         case SettingType_Rect: {
            QRect rect(req.rect().left(), req.rect().top(), req.rect().width()
               , req.rect().height());
            appSettings_->set(setting, rect);
         }
            break;
         case SettingType_Strings: {
            QStringList strings;
            for (const auto &s : req.strings().strings()) {
               strings << QString::fromStdString(s);
            }
            appSettings_->set(setting, strings);
         }
            break;
         case SettingType_StrMap: {
            QVariantMap map;
            for (const auto &keyVal : req.key_vals().key_vals()) {
               map[QString::fromStdString(keyVal.key())] = QString::fromStdString(keyVal.value());
            }
            appSettings_->set(setting, map);
         }
            break;
         default:
            logger_->error("[{}] unknown setting type: {}", __func__, (int)req.request().type());
            nbUpdates--;
            break;
         }
         nbUpdates++;
      }
      else {
         logger_->warn("[{}] unknown source for setting ({})", __func__
            , req.request().index());
         continue;
      }
   }
   if (nbUpdates) {
      SettingsMessage msg;
      *(msg.mutable_settings_updated()) = request;
      pushBroadcast(user_, msg.SerializeAsString());
      return ProcessingResult::Success;
   }
   return ProcessingResult::Ignored;
}

ProcessingResult SettingsAdapter::processArmoryServer(const BlockSettle::Terminal::SettingsMessage_ArmoryServer &request)
{
   int selIndex = 0;
   for (const auto &server : armoryServersProvider_->servers()) {
      if ((server.name == QString::fromStdString(request.server_name()))
         && (server.netType == static_cast<NetworkType>(request.network_type()))) {
         break;
      }
      selIndex++;
   }
   if (selIndex >= armoryServersProvider_->servers().size()) {
      logger_->error("[{}] failed to find Armory server {}", __func__, request.server_name());
      return ProcessingResult::Success;
   }
   armoryServersProvider_->setupServer(selIndex);
   appSettings_->selectNetwork();
}

ProcessingResult SettingsAdapter::processSetArmoryServer(const bs::message::Envelope& env, int index)
{
   armoryServersProvider_->setupServer(index);
   appSettings_->selectNetwork();
   return processGetArmoryServers(env);
}

ProcessingResult SettingsAdapter::processGetArmoryServers(const bs::message::Envelope& env)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_armory_servers();
   msgResp->set_idx_current(armoryServersProvider_->indexOfCurrent());
   msgResp->set_idx_connected(armoryServersProvider_->indexOfConnected());
   for (const auto& server : armoryServersProvider_->servers()) {
      auto msgSrv = msgResp->add_servers();
      msgSrv->set_network_type((int)server.netType);
      msgSrv->set_server_name(server.name.toStdString());
      msgSrv->set_server_address(server.armoryDBIp.toStdString());
      msgSrv->set_server_port(std::to_string(server.armoryDBPort));
      msgSrv->set_server_key(server.armoryDBKey.toStdString());
      msgSrv->set_run_locally(server.runLocally);
      msgSrv->set_one_way_auth(server.oneWayAuth_);
      msgSrv->set_password(server.password.toBinStr());
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

static ArmoryServer fromMessage(const SettingsMessage_ArmoryServer& msg)
{
   ArmoryServer result;
   result.name = QString::fromStdString(msg.server_name());
   result.netType = static_cast<NetworkType>(msg.network_type());
   result.armoryDBIp = QString::fromStdString(msg.server_address());
   result.armoryDBPort = std::stoi(msg.server_port());
   result.armoryDBKey = QString::fromStdString(msg.server_key());
   result.password = SecureBinaryData::fromString(msg.password());
   result.runLocally = msg.run_locally();
   result.oneWayAuth_ = msg.one_way_auth();
   return result;
}

ProcessingResult SettingsAdapter::processAddArmoryServer(const bs::message::Envelope& env
   , const SettingsMessage_ArmoryServer& request)
{
   const auto& server = fromMessage(request);
   if (armoryServersProvider_->add(server)) {
      armoryServersProvider_->setupServer(armoryServersProvider_->indexOf(server));
   }
   else {
      logger_->warn("[{}] failed to add server", __func__);
   }
   return processGetArmoryServers(env);
}

ProcessingResult SettingsAdapter::processDelArmoryServer(const bs::message::Envelope& env
   , int index)
{
   if (!armoryServersProvider_->remove(index)) {
      logger_->warn("[{}] failed to remove server #{}", __func__, index);
   }
   return processGetArmoryServers(env);
}

ProcessingResult SettingsAdapter::processUpdArmoryServer(const bs::message::Envelope& env
   , const SettingsMessage_ArmoryServerUpdate& request)
{
   const auto& server = fromMessage(request.server());
   if (!armoryServersProvider_->replace(request.index(), server)) {
      logger_->warn("[{}] failed to update server #{}", __func__, request.index());
   }
   return processGetArmoryServers(env);
}

ProcessingResult SettingsAdapter::processSignerSettings(const bs::message::Envelope &env)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_signer_response();
   msgResp->set_is_local(signersProvider_->currentSignerIsLocal());
   msgResp->set_network_type(appSettings_->get<int>(ApplicationSettings::netType));

   const auto &signerHost = signersProvider_->getCurrentSigner();
   msgResp->set_name(signerHost.name.toStdString());
   msgResp->set_host(signerHost.address.toStdString());
   msgResp->set_port(std::to_string(signerHost.port));
   msgResp->set_key(signerHost.key.toStdString());
   msgResp->set_id(signerHost.serverId());
   msgResp->set_remote_keys_dir(signersProvider_->remoteSignerKeysDir());
   msgResp->set_remote_keys_file(signersProvider_->remoteSignerKeysFile());

   for (const auto &signer : signersProvider_->signers()) {
      auto keyVal = msgResp->add_client_keys();
      keyVal->set_key(signer.serverId());
      keyVal->set_value(signer.key.toStdString());
   }
   msgResp->set_home_dir(appSettings_->GetHomeDir().toStdString());
   msgResp->set_auto_sign_spend_limit(appSettings_->get<double>(ApplicationSettings::autoSignSpendLimit));

   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult SettingsAdapter::processSignerSetKey(const SettingsMessage_SignerSetKey &request)
{
   signersProvider_->addKey(request.server_id(), request.new_key());
   return ProcessingResult::Success;
}

ProcessingResult SettingsAdapter::processSignerReset()
{
   signersProvider_->setupSigner(0, true);
   return ProcessingResult::Success;
}

ProcessingResult SettingsAdapter::processGetSigners(const bs::message::Envelope& env)
{
   SettingsMessage msg;
   auto msgResp = msg.mutable_signer_servers();
   msgResp->set_own_key(signersProvider_->remoteSignerOwnKey().toHexStr());
   msgResp->set_idx_current(signersProvider_->indexOfCurrent());
   for (const auto& signer : signersProvider_->signers()) {
      auto msgSrv = msgResp->add_servers();
      msgSrv->set_name(signer.name.toStdString());
      msgSrv->set_host(signer.address.toStdString());
      msgSrv->set_port(std::to_string(signer.port));
      msgSrv->set_key(signer.key.toStdString());
   }
   pushResponse(user_, env, msg.SerializeAsString());
   return ProcessingResult::Success;
}

ProcessingResult SettingsAdapter::processSetSigner(const bs::message::Envelope& env
   , int index)
{
   signersProvider_->setupSigner(index);
   return processGetSigners(env);
}

static SignerHost fromMessage(const SettingsMessage_SignerServer& msg)
{
   SignerHost result;
   result.name = QString::fromStdString(msg.name());
   result.address = QString::fromStdString(msg.host());
   result.port = std::stoi(msg.port());
   result.key = QString::fromStdString(msg.key());
   return result;
}

ProcessingResult SettingsAdapter::processAddSigner(const bs::message::Envelope& env
   , const SettingsMessage_SignerServer& request)
{
   const auto& signer = fromMessage(request);
   signersProvider_->add(signer);
   signersProvider_->setupSigner(signersProvider_->indexOf(signer));
   return processGetSigners(env);
}

ProcessingResult SettingsAdapter::processDelSigner(const bs::message::Envelope& env
   , int index)
{
   signersProvider_->remove(index);
   return processGetSigners(env);
}


void bs::message::setFromQVariant(const QVariant& val, SettingRequest* req, SettingResponse* resp)
{
   switch (val.type()) {
   case QVariant::Type::String:
      req->set_type(SettingType_String);
      resp->set_s(val.toString().toStdString());
      break;
   case QVariant::Type::Int:
      req->set_type(SettingType_Int);
      resp->set_i(val.toInt());
      break;
   case QVariant::Type::UInt:
      req->set_type(SettingType_UInt);
      resp->set_ui(val.toUInt());
      break;
   case QVariant::Type::ULongLong:
   case QVariant::Type::LongLong:
      req->set_type(SettingType_UInt64);
      resp->set_ui64(val.toULongLong());
      break;
   case QVariant::Type::Double:
      req->set_type(SettingType_Float);
      resp->set_f(val.toDouble());
      break;
   case QVariant::Type::Bool:
      req->set_type(SettingType_Bool);
      resp->set_b(val.toBool());
      break;
   case QVariant::Type::Rect:
      req->set_type(SettingType_Rect);
      {
         auto setRect = resp->mutable_rect();
         setRect->set_left(val.toRect().left());
         setRect->set_top(val.toRect().top());
         setRect->set_height(val.toRect().height());
         setRect->set_width(val.toRect().width());
      }
      break;
   case QVariant::Type::StringList:
      req->set_type(SettingType_Strings);
      for (const auto& s : val.toStringList()) {
         resp->mutable_strings()->add_strings(s.toStdString());
      }
      break;
   case QVariant::Type::Map:
      req->set_type(SettingType_StrMap);
      for (const auto& key : val.toMap().keys()) {
         auto kvData = resp->mutable_key_vals()->add_key_vals();
         kvData->set_key(key.toStdString());
         kvData->set_value(val.toMap()[key].toString().toStdString());
      }
      break;
   default: break;   // ignore other types
   }
}

QVariant bs::message::fromResponse(const BlockSettle::Terminal::SettingResponse& setting)
{
   QVariant value;
   switch (setting.request().type()) {
   case SettingType_String:
      value = QString::fromStdString(setting.s());
      break;
   case SettingType_Int:
      value = setting.i();
      break;
   case SettingType_UInt:
      value = setting.ui();
      break;
   case SettingType_UInt64:
      value = quint64(setting.ui64());
      break;
   case SettingType_Bool:
      value = setting.b();
      break;
   case SettingType_Float:
      value = setting.f();
      break;
   case SettingType_Rect:
      value = QRect(setting.rect().left(), setting.rect().top()
         , setting.rect().width(), setting.rect().height());
      break;
   case SettingType_Strings: {
      QStringList sl;
      for (const auto& s : setting.strings().strings()) {
         sl << QString::fromStdString(s);
      }
      value = sl;
   }
      break;
   case SettingType_StrMap: {
      QVariantMap vm;
      for (const auto& keyVal : setting.key_vals().key_vals()) {
         vm[QString::fromStdString(keyVal.key())] = QString::fromStdString(keyVal.value());
      }
      value = vm;
   }
      break;
   default: break;
   }
   return value;
}
