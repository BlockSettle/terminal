/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatOTCHelper.h"

#include <QFileDialog>
#include <spdlog/spdlog.h>

#include "ApplicationSettings.h"
#include "ArmoryConnection.h"
#include "ChatProtocol/ClientParty.h"
#include "OtcClient.h"
#include "OtcUtils.h"
#include "SignContainer.h"
#include "chat.pb.h"

ChatOTCHelper::ChatOTCHelper(QObject* parent /*= nullptr*/)
   : QObject(parent)
{
}

void ChatOTCHelper::init(bs::network::otc::Env env
   , const std::shared_ptr<spdlog::logger>& loggerPtr
   , const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
   , const std::shared_ptr<ArmoryConnection>& armory
   , const std::shared_ptr<SignContainer>& signContainer
   , const std::shared_ptr<AuthAddressManager> &authAddressManager
   , const std::shared_ptr<ApplicationSettings> &applicationSettings)
{
   loggerPtr_ = loggerPtr;

   OtcClientParams params;
   params.env = env;
   otcClient_ = new OtcClient(loggerPtr, walletsMgr, armory, signContainer, authAddressManager, std::move(params), this);
}

OtcClient* ChatOTCHelper::client() const
{
   return otcClient_;
}

void ChatOTCHelper::setCurrentUserId(const std::string& ownUserId)
{
   otcClient_->setOwnContactId(ownUserId);
}

void ChatOTCHelper::setGlobalOTCEntryTimeStamp(QDateTime timeStamp)
{
   selectedGlobalEntryTimeStamp_ = timeStamp;
}

QDateTime ChatOTCHelper::selectedGlobalOTCEntryTimeStamp() const
{
   return selectedGlobalEntryTimeStamp_;
}

void ChatOTCHelper::onLogout()
{
   for (const auto &contactId : connectedContacts_) {
      otcClient_->contactDisconnected(contactId);
   }
   connectedContacts_.clear();
}

void ChatOTCHelper::onProcessOtcPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response)
{
   otcClient_->processPbMessage(response);
}

void ChatOTCHelper::onOtcRequestSubmit(bs::network::otc::Peer *peer, const bs::network::otc::Offer& offer)
{
   if (!peer) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "peer not found");
      return;
   }

   bool result = otcClient_->sendOffer(peer, offer);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "send offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcPullOrReject(bs::network::otc::Peer *peer)
{
   if (!peer) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "peer not found");
      return;
   }

   bool result = otcClient_->pullOrReject(peer);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "pull or reject failed");
      return;
   }
}

void ChatOTCHelper::onOtcResponseAccept(bs::network::otc::Peer *peer, const bs::network::otc::Offer& offer)
{
   if (!peer) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "peer not found");
      return;
   }

   bool result = otcClient_->acceptOffer(peer, offer);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "accept offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcResponseUpdate(bs::network::otc::Peer *peer, const bs::network::otc::Offer& offer)
{
   if (!peer) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "peer not found");
      return;
   }

   bool result = otcClient_->updateOffer(peer, offer);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "update offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcResponseReject(bs::network::otc::Peer *peer)
{
   if (!peer) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "peer not found");
      return;
   }

   bool result = otcClient_->pullOrReject(peer);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "reject offer failed");
      return;
   }
}

void ChatOTCHelper::onOtcQuoteRequestSubmit(const bs::network::otc::QuoteRequest &request)
{
   bool result = otcClient_->sendQuoteRequest(request);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "sending quote request failed");
      return;
   }
}

void ChatOTCHelper::onOtcQuoteResponseSubmit(bs::network::otc::Peer *peer, const bs::network::otc::QuoteResponse &response)
{
   if (!peer) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "peer not found");
      return;
   }

   bool result = otcClient_->sendQuoteResponse(peer, response);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "sending response failed");
      return;
   }
}

void ChatOTCHelper::onMessageArrived(const Chat::MessagePtrList& messagePtr)
{
   for (const auto &msg : messagePtr) {
      if (msg->partyId() == Chat::OtcRoomName) {
         auto data = OtcUtils::deserializePublicMessage(msg->messageText());
         if (!data.isNull()) {
            otcClient_->processPublicMessage(msg->timestamp(), msg->senderHash(), data);
         }
      } else if (msg->partyMessageState() == Chat::SENT && msg->senderHash() != otcClient_->ownContactId()) {
         auto connIt = connectedContacts_.find(msg->senderHash());
         if (connIt == connectedContacts_.end()) {
            continue;
         }

         auto data = OtcUtils::deserializeMessage(msg->messageText());
         if (!data.isNull()) {
            otcClient_->processContactMessage(msg->senderHash(), data);
         }
      }
   }
}

void ChatOTCHelper::onPartyStateChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   const std::string& contactId = clientPartyPtr->userHash();
   auto connIt = connectedContacts_.find(contactId);
   if (clientPartyPtr->clientStatus() == Chat::ONLINE && connIt == connectedContacts_.end()) {
      otcClient_->contactConnected(contactId);
      connectedContacts_.insert(contactId);
   } else if (clientPartyPtr->clientStatus() == Chat::OFFLINE && connIt != connectedContacts_.end()) {
      otcClient_->contactDisconnected(contactId);
      connectedContacts_.erase(connIt);
   }
}
