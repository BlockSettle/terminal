/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SettingsAdapter.h"
#include <QRect>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "ArmoryServersProvider.h"
#include "Settings/SignersProvider.h"
#include "LogManager.h"
#include "PubKeyLoader.h"

#include "terminal.pb.h"

using namespace BlockSettle::Terminal;
using namespace bs::message;


SettingsAdapter::SettingsAdapter(const std::shared_ptr<ApplicationSettings> &settings
   , const QStringList &appArgs)
   : appSettings_(settings)
   , user_(std::make_shared<UserTerminal>(TerminalUsers::Settings))
   , userBC_(std::make_shared<UserTerminal>(TerminalUsers::BROADCAST))
{
   if (!appSettings_->LoadApplicationSettings(appArgs)) {
      throw std::runtime_error("failed to load settings");
   }
   logMgr_ = std::make_shared<bs::LogManager>();
   logMgr_->add(appSettings_->GetLogsConfig());
   logger_ = logMgr_->logger();

   if (!appSettings_->get<bool>(ApplicationSettings::initialized)) {
      appSettings_->SetDefaultSettings(true);
   }
   appSettings_->selectNetwork();
   logger_->debug("Settings loaded from {}", appSettings_->GetSettingsPath().toStdString());

   armoryServersProvider_ = std::make_shared<ArmoryServersProvider>(settings);
   signersProvider_ = std::make_shared<SignersProvider>(appSettings_);
}

bool SettingsAdapter::process(const bs::message::Envelope &env)
{
   if (env.receiver && (env.receiver->value<TerminalUsers>() == TerminalUsers::Settings)) {
      SettingsMessage msg;
      if (!msg.ParseFromString(env.message)) {
         logger_->error("[{}] failed to parse settings msg #{}", __func__, env.id);
         return true;
      }
      switch (msg.data_case()) {
      case SettingsMessage::kGetRequest:
         return processGetRequest(env, msg.get_request());
      case SettingsMessage::kPutRequest:
         return processPutRequest(msg.put_request());
      case SettingsMessage::kArmoryServer:
         return processArmoryServer(msg.armory_server());
      case SettingsMessage::kSignerRequest:
         return processSignerSettings(env);
      case SettingsMessage::kSignerSetKey:
         return processSignerSetKey(msg.signer_set_key());
      case SettingsMessage::kSignerReset:
         return processSignerReset();
      default:
         logger_->warn("[SettingsAdapter::process] unknown data case: {}"
            , msg.data_case());
         break;
      }
   }
   else if (env.sender->value<TerminalUsers>() == TerminalUsers::Blockchain) {
      //TODO: parse Armory request
   }
   return true;
}

bool SettingsAdapter::processRemoteSettings(uint64_t msgId)
{
   const auto &itReq = remoteSetReqs_.find(msgId);
   if (itReq == remoteSetReqs_.end()) {
      logger_->error("[{}] failed to find remote settings request #{}", __func__
         , msgId);
      return true;
   }
   return true;
}

bool SettingsAdapter::processGetRequest(const bs::message::Envelope &env
   , const SettingsMessage_SettingsRequest &request)
{
   unsigned int nbFetched = 0;
   SettingsMessage msg;
   auto msgResp = msg.mutable_get_response();
   for (const auto &req : request.requests()) {
      auto resp = msgResp->add_responses();
      resp->set_allocated_request(new SettingRequest(req));
      if (req.source() == SettingSource_Local) {
         const auto setting = static_cast<ApplicationSettings::Setting>(req.index());
         switch (req.type()) {
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
            const auto &rect = appSettings_->get<QRect>(setting);
            auto r = resp->mutable_rect();
            r->set_left(rect.left());
            r->set_top(rect.top());
            r->set_width(rect.width());
            r->set_height(rect.height());
         }
            break;
         case SettingType_Strings: {
            const auto &strings = appSettings_->get<QStringList>(setting);
            auto ss = resp->mutable_strings();
            for (const auto &string : strings) {
               ss->add_strings(string.toStdString());
            }
         }
            break;
         case SettingType_StrMap: {
            const auto &map = appSettings_->get<QVariantMap>(setting);
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
         nbFetched++;
      }
      else if (req.source() == SettingSource_Remote) {
         BsServerMessage msg;
         msg.mutable_network_settings_request();
         Envelope envReq{ 0, user_, UserTerminal::create(TerminalUsers::BsServer)
            , {}, {}, msg.SerializeAsString(), true };
         pushFill(envReq);
         remoteSetReqs_[envReq.id] = env;
         break;
      }
   }
   if (nbFetched > 0) {
      bs::message::Envelope envResp{ env.id, user_, env.sender, {}, {}
         , msg.SerializeAsString() };
      return pushFill(envResp);
   }
   return true;
}

bool SettingsAdapter::processPutRequest(const SettingsMessage_SettingsResponse &request)
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
            appSettings_->set(setting, req.ui64());
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
      else if (req.request().source() == SettingSource_Remote) {
         logger_->warn("[{}] remote settings ({}) are read-only", __func__
            , req.request().index());
         continue;
      }
   }
   if (nbUpdates) {
      SettingsMessage msg;
      *(msg.mutable_settings_updated()) = request;
      bs::message::Envelope env{ 0, user_, userBC_, bs::message::TimeStamp{}
         , bs::message::TimeStamp{}, msg.SerializeAsString() };
      return pushFill(env);
   }
   return true;
}

bool SettingsAdapter::processArmoryServer(const BlockSettle::Terminal::SettingsMessage_ArmoryServerSet &request)
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
      return true;
   }
   armoryServersProvider_->setupServer(selIndex);
   appSettings_->selectNetwork();
}

bool SettingsAdapter::processSignerSettings(const bs::message::Envelope &env)
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
   msgResp->set_local_port(appSettings_->get<std::string>(ApplicationSettings::localSignerPort));
   msgResp->set_home_dir(appSettings_->GetHomeDir().toStdString());
   msgResp->set_auto_sign_spend_limit(appSettings_->get<double>(ApplicationSettings::autoSignSpendLimit));

   bs::message::Envelope envResp{ env.id, user_, env.sender, {}, {}
      , msg.SerializeAsString() };
   return pushFill(envResp);
}

bool SettingsAdapter::processSignerSetKey(const SettingsMessage_SignerSetKey &request)
{
   signersProvider_->addKey(request.server_id(), request.new_key());
   return true;
}

bool SettingsAdapter::processSignerReset()
{
   signersProvider_->setupSigner(0, true);
   return true;
}
