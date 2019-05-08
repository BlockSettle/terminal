#include "CCFileManager.h"

#include "ApplicationSettings.h"
#include "CelerClient.h"
#include "ConnectionManager.h"
#include "EncryptionUtils.h"
#include "AuthSignManager.h"

#include <spdlog/spdlog.h>

#include <cassert>

#include <QFile>

#include "bs_communication.pb.h"

using namespace Blocksettle::Communication;

CCFileManager::CCFileManager(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<AuthSignManager> &authSignMgr
   , const std::shared_ptr<ConnectionManager>& connectionManager
   , const ZmqBIP15XDataConnection::cbNewKey &cb)
   : CCPubConnection(logger, connectionManager, cb)
   , appSettings_(appSettings)
   , authSignManager_(authSignMgr)
{
   connect(appSettings_.get(), &ApplicationSettings::settingChanged, this, &CCFileManager::onPubSettingsChanged
      , Qt::QueuedConnection);
}

void CCFileManager::onPubSettingsChanged(int setting, QVariant)
{
   if ((setting == ApplicationSettings::pubBridgeHost) || (setting == ApplicationSettings::pubBridgePort)) {
      RemoveAndDisableFileSave();
   }
}

void CCFileManager::RemoveAndDisableFileSave()
{
   saveToFileDisabled_ = true;
   const auto path = QString::fromStdString(appSettings_->get<std::string>(ApplicationSettings::ccFileName));
   if (QFile::exists(path)) {
      logger_->debug("[CCFileManager::RemoveAndDisableFileSave] remove {} and disable save"
         , path.toStdString());
      QFile::remove(path);
   } else {
      logger_->debug("[CCFileManager::RemoveAndDisableFileSave] disabling saving on cc gen file");
   }
}

bool CCFileManager::hasLocalFile() const
{
   const auto path = appSettings_->get<QString>(ApplicationSettings::ccFileName);
   return QFile(path).exists();
}

void CCFileManager::LoadSavedCCDefinitions()
{
   const auto &path = appSettings_->get<std::string>(ApplicationSettings::ccFileName);
   if (!LoadFromFile(path)) {
      emit LoadingFailed();
      QFile::remove(QString::fromStdString(path));
   }
}

void CCFileManager::ConnectToCelerClient(const std::shared_ptr<CelerClient> &celerClient)
{
   celerClient_ = celerClient;
}

bool CCFileManager::wasAddressSubmitted(const bs::Address &addr)
{
   return celerClient_->IsCCAddressSubmitted(addr.display());
}

void CCFileManager::FillFrom(Blocksettle::Communication::GetCCGenesisAddressesResponse *resp)
{
   ccSecurities_.clear();
   ccSecurities_.reserve(resp->ccsecurities_size());
   for (int i = 0; i < resp->ccsecurities_size(); i++) {
      const auto ccSecurity = resp->ccsecurities(i);
      bs::network::CCSecurityDef ccSecDef = {
         ccSecurity.securityid(), ccSecurity.product(), ccSecurity.description(),
         bs::Address(ccSecurity.genesisaddr()), ccSecurity.satoshisnb()
      };
      emit CCSecurityDef(ccSecDef);
      emit CCSecurityId(ccSecurity.securityid());
      emit CCSecurityInfo(QString::fromStdString(ccSecDef.product), QString::fromStdString(ccSecDef.description)
         , (unsigned long)ccSecDef.nbSatoshis, QString::fromStdString(ccSecurity.genesisaddr()));
      ccSecurities_.push_back(ccSecDef);
   }
   logger_->debug("[CCFileManager::ProcessCCGenAddressesResponse] got {} CC gen address[es]", ccSecurities_.size());

   currentRev_ = resp->revision();
   emit Loaded();
}

static inline AddressNetworkType networkType(const std::shared_ptr<ApplicationSettings> &settings)
{
   return (settings->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet)
      ? AddressNetworkType::TestNetType : AddressNetworkType::MainNetType;
}

void CCFileManager::ProcessGenAddressesResponse(const std::string& response, bool sigVerified, const std::string &sig)
{
   GetCCGenesisAddressesResponse genAddrResp;

   if (!genAddrResp.ParseFromString(response)) {
      logger_->error("[CCFileManager::ProcessCCGenAddressesResponse] data corrupted. Could not parse.");
      return;
   }
   if (!sigVerified) {
      logger_->error("[CCFileManager::ProcessCCGenAddressesResponse] rejecting unverified reply");
      return;
   }

   if (currentRev_ > 0) {
      if (genAddrResp.revision() == currentRev_) {
         logger_->debug("[CCFileManager::ProcessCCGenAddressesResponse] having the same revision already");
         return;
      }
      if (genAddrResp.revision() < currentRev_) {
         logger_->warn("[CCFileManager::ProcessCCGenAddressesResponse] PuB has more recent revision {} than we ({})"
            , genAddrResp.revision(), currentRev_);
      }
   }

   if (genAddrResp.networktype() != networkType(appSettings_)) {
      logger_->error("[CCFileManager::ProcessCCGenAddressesResponse] network type mismatch in reply: {}"
         , (int)genAddrResp.networktype());
      return;
   }

   FillFrom(&genAddrResp);
   SaveToFile(appSettings_->get<std::string>(ApplicationSettings::ccFileName), sig);
}

bool CCFileManager::SubmitAddressToPuB(const bs::Address &address, uint32_t seed)
{
   if (!celerClient_) {
      logger_->error("[CCFileManager::SubmitAddressToPuB] not connected");
      return false;
   }

   const auto cbSigned = [this, address](const std::string &data, const BinaryData &invisibleData, const std::string &signature) {
      SubmitAddrForInitialDistributionRequest addressRequest;
      if (!addressRequest.ParseFromString(invisibleData.toBinStr())) {
         logger_->error("[CCFileManager::SubmitAddressToPuB] failed to parse original request");
         emit CCSubmitFailed(QString::fromStdString(address.display()), tr("Failed to parse original request"));
         return;
      }
      if (addressRequest.prefixedaddress() != address.display()) {
         logger_->error("[CCFileManager::SubmitAddressToPuB] CC address mismatch");
         emit CCSubmitFailed(QString::fromStdString(address.display()), tr("CC address mismatch"));
         return;
      }

      RequestPacket  request;
      request.set_datasignature(signature);
      request.set_requesttype(SubmitCCAddrInitialDistribType);
      request.set_requestdata(data);

      logger_->debug("[CCFileManager::SubmitAddressToPuB] submitting addr {}", address.display());
      if (SubmitRequestToPB("submit_cc_addr", request.SerializeAsString())) {
         emit CCInitialSubmitted(QString::fromStdString(address.display()));
      }
      else {
         emit CCSubmitFailed(QString::fromStdString(address.display()), tr("Failed to send to PB"));
      }
   };

   const auto &cbSignFailed = [this, address](const QString &text) {
      logger_->error("[CCFileManager::SubmitAddressToPuB] failed to sign data: {}", text.toStdString());
      emit CCSubmitFailed(QString::fromStdString(address.display()), text);
   };

   SubmitAddrForInitialDistributionRequest addressRequest;
   addressRequest.set_username(celerClient_->userName());
   addressRequest.set_networktype(networkType(appSettings_));
   addressRequest.set_prefixedaddress(address.display());
   addressRequest.set_bsseed(seed);

   return authSignManager_->Sign(addressRequest.SerializeAsString(), tr("Private Market token")
      , tr("Submitting CC wallet address to receive PM token")
      , cbSigned, cbSignFailed, 90);
}

void CCFileManager::ProcessSubmitAddrResponse(const std::string& responseString)
{
   SubmitAddrForInitialDistributionResponse response;
   if (!response.ParseFromString(responseString)) {
      logger_->error("[CCFileManager::ProcessSubmitAddrResponse] failed to parse response");
      return;
   }

   bs::Address addr(response.prefixedaddress());

   if (!response.success()) {
      if (response.has_errormessage()) {
         logger_->error("[CCFileManager::ProcessSubmitAddrResponse] recv address {} rejected: {}"
            , addr.display(), response.errormessage());
      }
      else {
         logger_->error("[CCFileManager::ProcessSubmitAddrResponse] recv address {} rejected", addr.display());
      }
      return;
   }

   if (celerClient_->IsConnected()) {
      if (!celerClient_->SetCCAddressSubmitted(addr.display())) {
         logger_->warn("[CCFileManager::ProcessSubmitAddrResponse] failed to save address {} request event to Celer's user storage"
            , addr.display());
      }
   }

   logger_->debug("[CCFileManager::ProcessSubmitAddrResponse] {} succeeded", addr.display());

   emit CCAddressSubmitted(QString::fromStdString(addr.display()));
}

bool CCFileManager::LoadFromFile(const std::string &path)
{
   QFile f(QString::fromStdString(path));
   if (!f.exists()) {
      logger_->debug("[CCFileManager::LoadFromFile] no cc file to load at {}", path);
      return true;
   }
   if (!f.open(QIODevice::ReadOnly)) {
      logger_->error("[CCFileManager::LoadFromFile] failed to open file {} for reading", path);
      return false;
   }
   const auto buf = f.readAll();
   if (buf.isEmpty()) {
      logger_->error("[CCFileManager::LoadFromFile] failed to read from {}", path);
      return false;
   }

   GetCCGenesisAddressesResponse resp;
   if (!resp.ParseFromString(buf.toStdString())) {
      logger_->error("[CCFileManager::LoadFromFile] failed to parse {}", path);
      return false;
   }
   if (resp.networktype() != networkType(appSettings_)) {
      logger_->error("[CCFileManager::LoadFromFile] wrong network type in {}", path);
      return false;
   }

   if (!resp.has_signature()) {
      logger_->error("[CCFileManager::LoadFromFile] signature is missing in {}", path);
      return false;
   }
   const auto signature = resp.signature();
   resp.clear_signature();

   if (!VerifySignature(resp.SerializeAsString(), signature)) {
      logger_->error("[CCFileManager::LoadFromFile] signature verification failed for {}", path);
      return false;
   }

   FillFrom(&resp);
   return true;
}

bool CCFileManager::SaveToFile(const std::string &path, const std::string &sig)
{
   if (saveToFileDisabled_) {
      logger_->debug("[CCFileManager::SaveToFile] save to file disabled");
      return true;
   }

   GetCCGenesisAddressesResponse resp;

   resp.set_networktype(networkType(appSettings_));
   resp.set_revision(currentRev_);
   resp.set_signature(sig);

   for (const auto &ccDef : ccSecurities_) {
      const auto secDef = resp.add_ccsecurities();
      secDef->set_securityid(ccDef.securityId);
      secDef->set_product(ccDef.product);
      secDef->set_genesisaddr(ccDef.genesisAddr.display());
      secDef->set_satoshisnb(ccDef.nbSatoshis);
      if (!ccDef.description.empty()) {
         secDef->set_description(ccDef.description);
      }
   }

   QFile f(QString::fromStdString(path));
   if (!f.open(QIODevice::WriteOnly)) {
      logger_->error("[CCFileManager::SaveToFile] failed to open file {} for writing", path);
      return false;
   }
   const auto data = resp.SerializeAsString();
   if (data.size() != (size_t)f.write(data.data(), data.size())) {
      logger_->error("[CCFileManager::SaveToFile] failed to write to {}", path);
      return false;
   }
   return true;
}

bool CCFileManager::VerifySignature(const std::string& data, const std::string& signature) const
{
   const BinaryData publicKey = BinaryData::CreateFromHex(appSettings_->get<std::string>(ApplicationSettings::bsPublicKey));

   return CryptoECDSA().VerifyData(data, signature, publicKey);
}

std::string CCFileManager::GetPuBHost() const
{
   return appSettings_->get<std::string>(ApplicationSettings::pubBridgeHost);
}

std::string CCFileManager::GetPuBPort() const
{
   return appSettings_->get<std::string>(ApplicationSettings::pubBridgePort);
}

std::string CCFileManager::GetPuBKey() const
{
   return appSettings_->get<std::string>(ApplicationSettings::pubBridgePubKey);
}

bool CCFileManager::IsTestNet() const
{
   return appSettings_->get<NetworkType>(ApplicationSettings::netType) != NetworkType::MainNet;
}
