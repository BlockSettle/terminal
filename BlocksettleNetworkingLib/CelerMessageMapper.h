/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_MESSAGE_MAPPER_H__
#define __CELER_MESSAGE_MAPPER_H__

#include <string>

namespace CelerAPI
{

enum CelerMessageType
{
   CelerMessageTypeFirst,
   ConnectedEventType = CelerMessageTypeFirst,
   CreateApiSessionRequestType,
   CreateStandardUserType,
   CreateUserPropertyRequestType,
   UpdateUserPropertyRequestType,
   FindAllSocketsType,
   FindStandardUserType,
   GenerateResetUserPasswordTokenRequestType,
   HeartbeatType,
   LoginRequestType,
   LoginResponseType,
   LogoutMessageType,
   MultiResponseMessageType,
   ReconnectionFailedMessageType,
   ReconnectionRequestType,
   SingleResponseMessageType,
   ExceptionResponseMessageType,
   SocketConfigurationDownstreamEventType,
   StandardUserDownstreamEventType,
   ResetUserPasswordTokenType,
   ChangeUserPasswordRequestType,
   ChangeUserPasswordConfirmationType,
   FindUserPropertyByUsernameAndKeyType,
   UserPropertyDownstreamEventType,
   QuoteUpstreamType,
   QuoteNotificationType,
   QuoteDownstreamEventType,
   QuoteRequestNotificationType,
   QuoteRequestRejectDownstreamEventType,
   QuoteCancelRequestType,
   QuoteCancelNotificationType,
   QuoteCancelNotifReplyType,
   QuoteAckDownstreamEventType,
   CreateBitcoinOrderRequestType,
   CancelOrderRequestType,
   BitcoinOrderSnapshotDownstreamEventType,
   CreateFxOrderRequestType,
   FxOrderSnapshotDownstreamEventType,
   CreateOrderRequestRejectDownstreamEventType,
   QuoteCancelDownstreamEventType,
   FindAllAccountsType,
   GetAllAccountBalanceSnapshotType,
   AccountBulkUpdateDownstreamEventType,
   AccountBulkUpdateAcknowledgementType,
   AccountDownstreamEventType,
   AllSettlementAccountSnapshotDownstreamEventType,
   AccoutBalanceUpdatedDownstreamEventType,

   ProcessedFxTradeCaptureReportDownstreamEventType,
   ProcessedTradeCaptureReportAckType,

   SubscribeToTerminalRequestType,
   VerifiedAddressListUpdateEventType,
   SubscribeToTerminalResponseType,
   SessionEndedEventType,

   FindAssignedUserAccountsType,
   FindAllOrdersType,
   UserAccountDownstreamEventType,
   FindAllSubLedgersByAccountType,
   SubLedgerSnapshotDownstreamEventType,
   TransactionDownstreamEventType,

   VerifyXBTQuoteType,
   VerifyXBTQuoteRequestType,

   VerifyAuthenticationAddressResponseType,
   VerifyXBTQuoteResponseType,
   VerifyXBTQuoteRequestResponseType,

   ReserveCashForXBTRequestType,
   ReserveCashForXBTResponseType,

   XBTTradeRequestType,
   XBTTradeResponseType,

   FxTradeRequestType,
   FxTradeResponseType,

   ColouredCoinTradeRequestType,
   ColouredCoinTradeResponseType,

   VerifyColouredCoinQuoteType,
   VerifyColouredCoinQuoteRequestType,
   VerifyColouredCoinAcceptedQuoteType,

   VerifyColouredCoinQuoteResponseType,
   VerifyColouredCoinQuoteRequestResponseType,
   VerifyColouredCoinAcceptedQuoteResponseType,

   SignTransactionNotificationType,
   SignTransactionRequestType,

   EndOfDayPriceReportType,

   XBTTradeStatusRequestType,
   ColouredCoinTradeStatusRequestType,

   PersistenceExceptionType,

   MarketDataSubscriptionRequestType,
   MarketDataFullSnapshotDownstreamEventType,
   MarketDataRequestRejectDownstreamEventType,

   MarketStatisticSnapshotDownstreamEventType,
   MarketStatisticRequestType,

   CreateSecurityDefinitionRequestType,
   CreateWarehouseConfigurationRequestType,
   WarehouseConfigurationDownstreamEventType,

   CreateSecurityListingRequestType,
   SecurityListingDownstreamEventType,

   FindAllSecurityDefinitionsType,
   SecurityDefinitionDownstreamEventType,

   FindAllSecurityListingsRequestType,

   CelerMessageTypeLast,
   UndefinedType = CelerMessageTypeLast
};

std::string GetMessageClass(CelerMessageType messageType);

CelerMessageType GetMessageType(const std::string& fullClassName);

bool isValidMessageType(CelerMessageType messageType);

};

#endif // __CELER_MESSAGE_MAPPER_H__
