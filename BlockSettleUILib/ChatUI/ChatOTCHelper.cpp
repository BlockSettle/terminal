#include "ChatOTCHelper.h"

#include <QFileDialog>
#include <spdlog/spdlog.h>

#include "ApplicationSettings.h"
#include "ArmoryConnection.h"
#include "OtcClient.h"
#include "OtcUtils.h"
#include "SignContainer.h"
#include "chat.pb.h"

ChatOTCHelper::ChatOTCHelper(QObject* parent /*= nullptr*/)
   : QObject(parent)
{
}

void ChatOTCHelper::init(const std::shared_ptr<spdlog::logger>& loggerPtr
   , const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
   , const std::shared_ptr<ArmoryConnection>& armory
   , const std::shared_ptr<SignContainer>& signContainer
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<ApplicationSettings> &applicationSettings)
{
   loggerPtr_ = loggerPtr;

   OtcClientParams params;

   params.offlineLoadPathCb = [applicationSettings]() -> std::string {
      QString signerOfflineDir = applicationSettings->get<QString>(ApplicationSettings::signerOfflineDir);

      QString filePath = QFileDialog::getOpenFileName(nullptr, tr("Select Transaction file"), signerOfflineDir
         , tr("TX files (*.bin)"));

      if (!filePath.isEmpty()) {
         // Update latest used directory if needed
         QString newSignerOfflineDir = QFileInfo(filePath).absoluteDir().path();
         if (signerOfflineDir != newSignerOfflineDir) {
            applicationSettings->set(ApplicationSettings::signerOfflineDir, newSignerOfflineDir);
         }
      }

      return filePath.toStdString();
   };

   params.offlineSavePathCb = [applicationSettings](const std::string &walletId) -> std::string {
      QString signerOfflineDir = applicationSettings->get<QString>(ApplicationSettings::signerOfflineDir);

      const qint64 timestamp = QDateTime::currentDateTime().toSecsSinceEpoch();
      const std::string fileName = fmt::format("{}_{}.bin", walletId, timestamp);

      QString defaultFilePath = QDir(signerOfflineDir).filePath(QString::fromStdString(fileName));
      QString filePath = QFileDialog::getSaveFileName(nullptr, tr("Save Offline TX as..."), defaultFilePath);

      if (!filePath.isEmpty()) {
         QString newSignerOfflineDir = QFileInfo(filePath).absoluteDir().path();
         if (signerOfflineDir != newSignerOfflineDir) {
            applicationSettings->set(ApplicationSettings::signerOfflineDir, newSignerOfflineDir);
         }
      }

      return filePath.toStdString();
   };

   otcClient_ = new OtcClient(loggerPtr, walletsMgr, armory, signContainer, authAddressManager, std::move(params), this);
}

OtcClient* ChatOTCHelper::getClient() const
{
   return otcClient_;
}

void ChatOTCHelper::setCurrentUserId(const std::string& ownUserId)
{
   otcClient_->setCurrentUserId(ownUserId);
}

const bs::network::otc::Peer* ChatOTCHelper::getPeer(const std::string& partyId) const
{
   return otcClient_->peer(partyId);
}

void ChatOTCHelper::onLogout()
{
   for (const auto &partyId : connectedPeers_) {
      otcClient_->peerDisconnected(partyId);
   }
   connectedPeers_.clear();
}

void ChatOTCHelper::onProcessOtcPbMessage(const std::string& data)
{
   otcClient_->processPbMessage(data);
}

void ChatOTCHelper::onOtcRequestSubmit(const std::string& partyId, const bs::network::otc::Offer& offer)
{
   bool result = otcClient_->sendOffer(offer, partyId);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "send offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcRequestPull(const std::string& partyId)
{
   bool result = otcClient_->pullOrRejectOffer(partyId);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "pull offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcResponseAccept(const std::string& partyId, const bs::network::otc::Offer& offer)
{
   bool result = otcClient_->acceptOffer(offer, partyId);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "accept offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcResponseUpdate(const std::string& partyId, const bs::network::otc::Offer& offer)
{
   bool result = otcClient_->updateOffer(offer, partyId);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "update offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcResponseReject(const std::string& partyId)
{
   bool result = otcClient_->pullOrRejectOffer(partyId);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "reject offer failed");
      return;
   }
}

void ChatOTCHelper::onMessageArrived(const Chat::MessagePtrList& messagePtr)
{
   for (const auto &msg : messagePtr) {
      if (msg->partyMessageState() == Chat::SENT && msg->senderHash() != otcClient_->getCurrentUser()) {
         
         auto connIt = connectedPeers_.find(msg->partyId());
         if (connIt == connectedPeers_.end()) {
            continue;
         }

         auto data = OtcUtils::deserializeMessage(msg->messageText());
         if (!data.isNull()) {
            otcClient_->processMessage(msg->partyId(), data);
         }
      }
   }
}

void ChatOTCHelper::onPartyStateChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   if (clientPartyPtr->partyType() != Chat::PRIVATE_DIRECT_MESSAGE) {
      return;
   }
   
   const std::string& partyId = clientPartyPtr->id();
   auto connIt = connectedPeers_.find(partyId);
   if (clientPartyPtr->clientStatus() == Chat::ONLINE && connIt == connectedPeers_.end()) {
      otcClient_->peerConnected(partyId);
      connectedPeers_.insert(partyId);
   } else if (clientPartyPtr->clientStatus() == Chat::OFFLINE && connIt != connectedPeers_.end()) {
      otcClient_->peerDisconnected(partyId);
      connectedPeers_.erase(connIt);
   }
}
