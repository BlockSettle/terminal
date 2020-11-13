/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SETTLEMENT_ADAPTER_H
#define SETTLEMENT_ADAPTER_H

#include "CommonTypes.h"
#include "CoreWallet.h"
#include "Message/Adapter.h"
#include "PasswordDialogData.h"

namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Common {
      class ArmoryMessage_ZCReceived;
      class SignerMessage_SignTxResponse;
      class WalletsMessage_XbtTxResponse;
   }
   namespace Terminal {
      class AcceptRFQ;
      class BsServerMessage_SignXbtHalf;
      class IncomingRFQ;
      class MatchingMessage_Order;
      class Quote;
      class SettlementMessage_SendRFQ;
   }
}

class SettlementAdapter : public bs::message::Adapter
{
public:
   SettlementAdapter(const std::shared_ptr<spdlog::logger> &);
   ~SettlementAdapter() override = default;

   bool process(const bs::message::Envelope &) override;

   std::set<std::shared_ptr<bs::message::User>> supportedReceivers() const override {
      return { user_ };
   }
   std::string name() const override { return "Settlement"; }

private:
   bool processZC(const BlockSettle::Common::ArmoryMessage_ZCReceived&);
   bool processMatchingQuote(const BlockSettle::Terminal::Quote&);
   bool processMatchingOrder(const BlockSettle::Terminal::MatchingMessage_Order&);
   bool processMatchingInRFQ(const BlockSettle::Terminal::IncomingRFQ&);

   bool processBsUnsignedPayin(const BinaryData& settlementId);
   bool processBsSignPayin(const BlockSettle::Terminal::BsServerMessage_SignXbtHalf&);
   bool processBsSignPayout(const BlockSettle::Terminal::BsServerMessage_SignXbtHalf&);

   bool processCancelRFQ(const std::string& rfqId);
   bool processAcceptRFQ(const bs::message::Envelope&
      , const BlockSettle::Terminal::AcceptRFQ&);
   bool processSendRFQ(const bs::message::Envelope&
      , const BlockSettle::Terminal::SettlementMessage_SendRFQ&);
   bool processXbtTx(uint64_t msgId, const BlockSettle::Common::WalletsMessage_XbtTxResponse&);
   bool processSignedTx(uint64_t msgId, const BlockSettle::Common::SignerMessage_SignTxResponse&);
   bool processHandshakeTimeout(const std::string& id);
   bool processInRFQTimeout(const std::string& id);

   void startXbtSettlement(const bs::network::Quote&);
   void startCCSettlement(const bs::network::Quote&);
   void unreserve(const std::string& id, const std::string& subId = {});
   void cancel(const BinaryData& settlementId);
   void close(const BinaryData& settlementId);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::message::User>  user_, userMtch_, userWallets_, userBS_;
   std::shared_ptr<bs::message::User>  userSigner_;

   struct Settlement {
      bs::message::Envelope   env;
      bool  dealer{ false };
      bs::network::RFQ     rfq;
      std::string          reserveId;
      bs::network::Quote   quote;
      std::string          fxProduct;
      bs::XBTAmount        amount;
      double               actualXbtPrice;
      bool  dealerAddrValidationReqd{ false };
      BinaryData           settlementId;
      bs::Address          settlementAddress;
      std::string          txComment;
      bs::Address          recvAddress;
      bs::Address          ownAuthAddr;
      BinaryData           ownKey;
      BinaryData           counterKey;
      bs::Address          counterAuthAddr;
      bs::core::wallet::TXSignRequest  payin;
      bool  otc{ false };
      bool  handshakeComplete{ false };
   };
   std::unordered_map<std::string, std::shared_ptr<Settlement>>   settlByRfqId_;
   std::unordered_map<std::string, std::shared_ptr<Settlement>>   settlByQuoteId_;
   std::map<BinaryData, std::shared_ptr<Settlement>>              settlBySettlId_;
   std::map<BinaryData, BinaryData>                               pendingZCs_;

   std::map<uint64_t, BinaryData>   payinRequests_;
   std::map<uint64_t, BinaryData>   payoutRequests_;

private:
   bs::sync::PasswordDialogData getDialogData(const QDateTime& timestamp
      , const Settlement &) const;
   bs::sync::PasswordDialogData getPayinDialogData(const QDateTime& timestamp
      , const Settlement&) const;
   bs::sync::PasswordDialogData getPayoutDialogData(const QDateTime& timestamp
      , const Settlement&) const;
};


#endif	// SETTLEMENT_ADAPTER_H
