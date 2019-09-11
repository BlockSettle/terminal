#include "ChatOTCHelper.h"

#include <spdlog/spdlog.h>
#include "ArmoryConnection.h"
#include "SignContainer.h"

#include "OtcClient.h"
#include "OtcUtils.h"
#include "chat.pb.h"

ChatOTCHelper::ChatOTCHelper(QObject* parent /*= nullptr*/)
   : QObject(parent)
{
}

void ChatOTCHelper::init(const std::shared_ptr<spdlog::logger>& loggerPtr
   , const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
   , const std::shared_ptr<ArmoryConnection>& armory
   , const std::shared_ptr<SignContainer>& signContainer)
{
   loggerPtr_ = loggerPtr;
   otcClient_ = new OtcClient(loggerPtr, walletsMgr, armory, signContainer, this);
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
      if (msg->partyMessageState() == Chat::SENT) {
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

   if (clientPartyPtr->clientStatus() == Chat::ONLINE) {
      otcClient_->peerConnected(partyId);
      connectedPeers_.insert(partyId);
      return;
   }

   otcClient_->peerDisconnected(partyId);
   connectedPeers_.erase(partyId);
}
