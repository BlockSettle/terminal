/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerMessageMapper.h"

#include <unordered_map>

namespace CelerAPI {

static const std::unordered_map<std::string, CelerMessageType> nameToType = {
   { "com.blocksettle.private_bridge.accounts.DownstreamPrivateBridgeAccountProto$AccountBulkUpdateDownstreamEvent", AccountBulkUpdateDownstreamEventType},
   { "com.blocksettle.private_bridge.accounts.DownstreamPrivateBridgeAccountProto$AccoutBalanceUpdatedDownstreamEvent", AccoutBalanceUpdatedDownstreamEventType},
   { "com.blocksettle.private_bridge.accounts.DownstreamPrivateBridgeAccountProto$AllSettlementAccountSnapshotDownstreamEvent", AllSettlementAccountSnapshotDownstreamEventType},
   { "com.blocksettle.private_bridge.accounts.UpstreamPrivateBridgeAccountProto$FindAllAccounts", FindAllAccountsType},
   { "com.blocksettle.private_bridge.accounts.UpstreamPrivateBridgeAccountProto$GetAllAccountBalanceSnapshot", GetAllAccountBalanceSnapshotType},
   { "com.blocksettle.private_bridge.accounts.UpstreamPrivateBridgeAccountProto$AccountBulkUpdateAcknowledgement", AccountBulkUpdateAcknowledgementType},
   { "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$SessionEndedEvent", SessionEndedEventType },
   { "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$SubscribeToTerminalRequest", SubscribeToTerminalRequestType },
   { "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$SubscribeToTerminalResponse", SubscribeToTerminalResponseType },
   { "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$VerifiedAddressListUpdateEvent", VerifiedAddressListUpdateEventType },
   { "com.celertech.baseserver.api.session.DownstreamSocketProto$SocketConfigurationDownstreamEvent", SocketConfigurationDownstreamEventType },
   { "com.celertech.baseserver.api.session.UpstreamSessionProto$CreateApiSessionRequest", CreateApiSessionRequestType },
   { "com.celertech.baseserver.api.socket.UpstreamSocketProto$FindAllSockets", FindAllSocketsType },
   { "com.celertech.baseserver.api.user.DownstreamAuthenticationUserProto$ChangeUserPasswordConfirmation", ChangeUserPasswordConfirmationType },
   { "com.celertech.baseserver.api.user.DownstreamAuthenticationUserProto$ResetUserPasswordToken", ResetUserPasswordTokenType },
   { "com.celertech.baseserver.api.user.DownstreamAuthenticationUserProto$StandardUserDownstreamEvent", StandardUserDownstreamEventType },
   { "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$ChangeUserPasswordRequest", ChangeUserPasswordRequestType },
   { "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$CreateStandardUser", CreateStandardUserType },
   { "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$FindStandardUser", FindStandardUserType },
   { "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$GenerateResetUserPasswordTokenRequest", GenerateResetUserPasswordTokenRequestType },
   { "com.celertech.baseserver.communication.login.DownstreamLoginProto$LoginResponse", LoginResponseType },
   { "com.celertech.baseserver.communication.login.DownstreamLoginProto$LogoutMessage", LogoutMessageType },
   { "com.celertech.baseserver.communication.login.UpstreamLoginProto$LoginRequest", LoginRequestType },
   { "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ConnectedEvent", ConnectedEventType },
   { "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$Heartbeat", HeartbeatType },
   { "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$MultiResponseMessage", MultiResponseMessageType },
   { "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ReconnectionFailedMessage", ReconnectionFailedMessageType },
   { "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ReconnectionRequest", ReconnectionRequestType },
   { "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$SingleResponseMessage", SingleResponseMessageType },
   { "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ExceptionResponseMessage", ExceptionResponseMessageType },
   { "com.celertech.clearing.api.tradecapturereport.processed.DownstreamProcessedTradeCaptureProto$ProcessedFxTradeCaptureReportDownstreamEvent", ProcessedFxTradeCaptureReportDownstreamEventType},
   { "com.celertech.clearing.api.tradecapturereport.processed.DownstreamProcessedTradeCaptureProto$ProcessedTradeCaptureReportAck", ProcessedTradeCaptureReportAckType},
   { "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteNotification", QuoteNotificationType },
   { "com.celertech.marketmerchant.api.order.DownstreamOrderProto$BitcoinOrderSnapshotDownstreamEvent", BitcoinOrderSnapshotDownstreamEventType },
   { "com.celertech.marketmerchant.api.order.UpstreamOrderProto$CreateBitcoinOrderRequest", CreateBitcoinOrderRequestType },
   { "com.celertech.marketmerchant.api.order.UpstreamOrderProto$CancelOrderRequest", CancelOrderRequestType },
   { "com.celertech.marketmerchant.api.order.UpstreamOrderProto$CreateFxOrderRequest", CreateFxOrderRequestType },
   { "com.celertech.marketmerchant.api.order.DownstreamOrderProto$FxOrderSnapshotDownstreamEvent", FxOrderSnapshotDownstreamEventType },
   { "com.celertech.marketmerchant.api.order.DownstreamOrderProto$CreateOrderRequestRejectDownstreamEvent", CreateOrderRequestRejectDownstreamEventType },
   { "com.celertech.marketmerchant.api.order.UpstreamOrderProto$FindAllOrderSnapshotsBySessionKey", FindAllOrdersType },
   { "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteCancelDownstreamEvent", QuoteCancelDownstreamEventType },
   { "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteDownstreamEvent", QuoteDownstreamEventType },
   { "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteRequestNotification", QuoteRequestNotificationType },
   { "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteRequestRejectDownstreamEvent", QuoteRequestRejectDownstreamEventType },
   { "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteRequest", QuoteUpstreamType },
   { "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteCancelRequest", QuoteCancelRequestType },
   { "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteCancelNotification", QuoteCancelNotificationType },
   { "com.celertech.marketwarehouse.api.quote.DownstreamQuoteProto$QuoteCancelDownstreamEvent", QuoteCancelNotifReplyType },
   { "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteAcknowledgementDownstreamEvent", QuoteAckDownstreamEventType },
   { "com.celertech.marketmerchant.api.order.bitcoin.DownstreamBitcoinTransactionSigningProto$SignTransactionNotification", SignTransactionNotificationType },
   { "com.celertech.marketmerchant.api.order.bitcoin.UpstreamBitcoinTransactionSigningProto$SignTransactionRequest", SignTransactionRequestType },
   { "com.celertech.piggybank.api.generalledger.DownstreamGeneralLedgerProto$TransactionDownstreamEvent", TransactionDownstreamEventType },
   { "com.celertech.piggybank.api.subledger.DownstreamSubLedgerProto$SubLedgerSnapshotDownstreamEvent", SubLedgerSnapshotDownstreamEventType },
   { "com.celertech.piggybank.api.subledger.UpstreamSubLedgerProto$FindAllSubLedgersByAccount", FindAllSubLedgersByAccountType },
   { "com.celertech.staticdata.api.account.DownstreamAccountProto$AccountDownstreamEvent", AccountDownstreamEventType},
   { "com.celertech.staticdata.api.user.account.DownstreamUserAccountProto$UserAccountDownstreamEvent", UserAccountDownstreamEventType },
   { "com.celertech.staticdata.api.user.account.UpstreamUserAccountProto$FindAssignedUserAccounts", FindAssignedUserAccountsType },
   { "com.celertech.staticdata.api.user.property.DownstreamUserPropertyProto$UserPropertyDownstreamEvent", UserPropertyDownstreamEventType },
   { "com.celertech.staticdata.api.user.property.UpstreamUserPropertyProto$CreateUserPropertyRequest", CreateUserPropertyRequestType},
   { "com.celertech.staticdata.api.user.property.UpstreamUserPropertyProto$FindUserPropertyByUsernameAndKey", FindUserPropertyByUsernameAndKeyType },
   { "com.celertech.staticdata.api.user.property.UpstreamUserPropertyProto$UpdateUserPropertyRequest", UpdateUserPropertyRequestType},
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyXBTQuoteRequest", VerifyXBTQuoteRequestType },
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyXBTQuote", VerifyXBTQuoteType },
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$XBTTradeRequest", XBTTradeRequestType },
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$ReserveCashForXBTRequest", ReserveCashForXBTRequestType },
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyAuthenticationAddressResponse", VerifyAuthenticationAddressResponseType},
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyXBTQuoteResponse", VerifyXBTQuoteResponseType},
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyXBTQuoteRequestResponse", VerifyXBTQuoteRequestResponseType},
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$XBTTradeResponse", XBTTradeResponseType},
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$ReserveCashForXBTResponse", ReserveCashForXBTResponseType},
   { "com.blocksettle.private_bridge.spotfx.UpstreamSpotFXProto$FxTradeRequest", FxTradeRequestType },
   { "com.blocksettle.private_bridge.spotfx.DownstreamSpotFXProto$FxTradeResponse", FxTradeResponseType },
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyColouredCoinQuote", VerifyColouredCoinQuoteType },
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyColouredCoinQuoteRequest", VerifyColouredCoinQuoteRequestType },
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyColouredCoinAcceptedQuote", VerifyColouredCoinAcceptedQuoteType },
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$ColouredCoinTradeRequest", ColouredCoinTradeRequestType },
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyColouredCoinQuoteResponse", VerifyColouredCoinQuoteResponseType},
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyColouredCoinQuoteRequestResponse", VerifyColouredCoinQuoteRequestResponseType},
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyColouredCoinAcceptedQuoteResponse", VerifyColouredCoinAcceptedQuoteResponseType},
   { "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$ColouredCoinTradeResponse", ColouredCoinTradeResponseType},
   { "com.blocksettle.private_bridge.eod.UpstreamEoDProto$EndOfDayPriceReport", EndOfDayPriceReportType},
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$XBTTradeStatusRequest", XBTTradeStatusRequestType},
   { "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$ColouredCoinTradeStatusRequest", ColouredCoinTradeStatusRequestType},
   { "com.celertech.baseserver.api.exception.DownstreamExceptionProto$PersistenceException", PersistenceExceptionType},
   { "com.celertech.marketdata.api.price.UpstreamPriceProto$MarketDataSubscriptionRequest", MarketDataSubscriptionRequestType},
   { "com.celertech.marketdata.api.price.DownstreamPriceProto$MarketDataFullSnapshotDownstreamEvent", MarketDataFullSnapshotDownstreamEventType},
   { "com.celertech.marketdata.api.price.DownstreamPriceProto$MarketDataRequestRejectDownstreamEvent", MarketDataRequestRejectDownstreamEventType},
   { "com.celertech.marketdata.api.marketstatistic.DownstreamMarketStatisticProto$MarketStatisticSnapshotDownstreamEvent", MarketStatisticSnapshotDownstreamEventType},
   { "com.celertech.marketdata.api.marketstatistic.UpstreamMarketStatisticProto$MarketStatisticRequest", MarketStatisticRequestType},
   { "com.celertech.marketmerchant.api.securitydefinition.UpstreamSecurityDefinitionProto$CreateSecurityDefinition", CreateSecurityDefinitionRequestType},
   { "com.celertech.marketwarehouse.api.configuration.UpstreamWarehouseConfigurationProto$CreateWarehouseConfigurationRequest", CreateWarehouseConfigurationRequestType},
   { "com.celertech.marketwarehouse.api.configuration.DownstreamWarehouseConfigurationProto$WarehouseConfigurationDownstreamEvent", WarehouseConfigurationDownstreamEventType},
   { "com.celertech.staticdata.api.security.UpstreamSecurityProto$CreateSecurityListingRequest", CreateSecurityListingRequestType},
   { "com.celertech.staticdata.api.security.DownstreamSecurityProto$SecurityListingDownstreamEvent", SecurityListingDownstreamEventType},
   { "com.celertech.marketmerchant.api.securitydefinition.UpstreamSecurityDefinitionProto$FindAllSecurityDefinitions", FindAllSecurityDefinitionsType},
   { "com.celertech.marketmerchant.api.securitydefinition.DownstreamSecurityDefinitionProto$SecurityDefinitionDownstreamEvent", SecurityDefinitionDownstreamEventType},
   { "com.celertech.staticdata.api.security.UpstreamSecurityProto$FindAllSecurityListingsRequest", FindAllSecurityListingsRequestType}
};

static const std::unordered_map<int, std::string> typeToName = {
   { SocketConfigurationDownstreamEventType, "com.celertech.baseserver.api.session.DownstreamSocketProto$SocketConfigurationDownstreamEvent" },
   { CreateApiSessionRequestType, "com.celertech.baseserver.api.session.UpstreamSessionProto$CreateApiSessionRequest" },
   { FindAllAccountsType, "com.blocksettle.private_bridge.accounts.UpstreamPrivateBridgeAccountProto$FindAllAccounts"},
   { GetAllAccountBalanceSnapshotType, "com.blocksettle.private_bridge.accounts.UpstreamPrivateBridgeAccountProto$GetAllAccountBalanceSnapshot"},
   { AccountBulkUpdateAcknowledgementType, "com.blocksettle.private_bridge.accounts.UpstreamPrivateBridgeAccountProto$AccountBulkUpdateAcknowledgement"},
   { FindAllSocketsType, "com.celertech.baseserver.api.socket.UpstreamSocketProto$FindAllSockets" },
   { StandardUserDownstreamEventType, "com.celertech.baseserver.api.user.DownstreamAuthenticationUserProto$StandardUserDownstreamEvent" },
   { CreateStandardUserType, "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$CreateStandardUser" },
   { FindStandardUserType, "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$FindStandardUser" },
   { GenerateResetUserPasswordTokenRequestType, "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$GenerateResetUserPasswordTokenRequest" },
   { ChangeUserPasswordRequestType, "com.celertech.baseserver.api.user.UpstreamAuthenticationUserProto$ChangeUserPasswordRequest" },
   { LoginResponseType, "com.celertech.baseserver.communication.login.DownstreamLoginProto$LoginResponse" },
   { LogoutMessageType, "com.celertech.baseserver.communication.login.DownstreamLoginProto$LogoutMessage" },
   { LoginRequestType, "com.celertech.baseserver.communication.login.UpstreamLoginProto$LoginRequest" },
   { ConnectedEventType, "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ConnectedEvent" },
   { HeartbeatType, "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$Heartbeat" },
   { MultiResponseMessageType, "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$MultiResponseMessage" },
   { ReconnectionFailedMessageType, "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ReconnectionFailedMessage" },
   { ReconnectionRequestType, "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ReconnectionRequest" },
   { SingleResponseMessageType, "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$SingleResponseMessage" },
   { ExceptionResponseMessageType, "com.celertech.baseserver.communication.netty.protobuf.NettyCommunication$ExceptionResponseMessage"},
   { FindAllOrdersType, "com.celertech.marketmerchant.api.order.UpstreamOrderProto$FindAllOrderSnapshotsBySessionKey" },
   { CreateUserPropertyRequestType, "com.celertech.staticdata.api.user.property.UpstreamUserPropertyProto$CreateUserPropertyRequest"},
   { UpdateUserPropertyRequestType, "com.celertech.staticdata.api.user.property.UpstreamUserPropertyProto$UpdateUserPropertyRequest"},
   { ResetUserPasswordTokenType, "com.celertech.baseserver.api.user.DownstreamAuthenticationUserProto$ResetUserPasswordToken" },
   { ChangeUserPasswordConfirmationType, "com.celertech.baseserver.api.user.DownstreamAuthenticationUserProto$ChangeUserPasswordConfirmation" },
   { FindUserPropertyByUsernameAndKeyType, "com.celertech.staticdata.api.user.property.UpstreamUserPropertyProto$FindUserPropertyByUsernameAndKey"},
   { UserPropertyDownstreamEventType, "com.celertech.staticdata.api.user.property.DownstreamUserPropertyProto$UserPropertyDownstreamEvent"},
   { QuoteUpstreamType, "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteRequest"},
   { QuoteCancelRequestType, "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteCancelRequest" },
   { QuoteCancelNotificationType, "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteCancelNotification" },
   { QuoteNotificationType, "com.celertech.marketmerchant.api.quote.UpstreamQuoteProto$QuoteNotification" },
   { QuoteDownstreamEventType, "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteDownstreamEvent"},
   { QuoteRequestNotificationType, "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteRequestNotification" },
   { QuoteRequestRejectDownstreamEventType, "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteRequestRejectDownstreamEvent" },
   { CreateBitcoinOrderRequestType, "com.celertech.marketmerchant.api.order.UpstreamOrderProto$CreateBitcoinOrderRequest"},
   { CancelOrderRequestType, "com.celertech.marketmerchant.api.order.UpstreamOrderProto$CancelOrderRequest"},
   { BitcoinOrderSnapshotDownstreamEventType, "com.celertech.marketmerchant.api.order.DownstreamOrderProto$BitcoinOrderSnapshotDownstreamEvent"},
   { CreateFxOrderRequestType, "com.celertech.marketmerchant.api.order.UpstreamOrderProto$CreateFxOrderRequest" },
   { FxOrderSnapshotDownstreamEventType, "com.celertech.marketmerchant.api.order.DownstreamOrderProto$FxOrderSnapshotDownstreamEvent" },
   { CreateOrderRequestRejectDownstreamEventType, "com.celertech.marketmerchant.api.order.DownstreamOrderProto$CreateOrderRequestRejectDownstreamEvent" },
   { QuoteCancelDownstreamEventType, "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteCancelDownstreamEvent" },
   { QuoteCancelNotifReplyType, "com.celertech.marketwarehouse.api.quote.DownstreamQuoteProto$QuoteCancelDownstreamEvent" },
   { QuoteAckDownstreamEventType, "com.celertech.marketmerchant.api.quote.DownstreamQuoteProto$QuoteAcknowledgementDownstreamEvent" },
   { SignTransactionNotificationType, "com.celertech.marketmerchant.api.order.bitcoin.DownstreamBitcoinTransactionSigningProto$SignTransactionNotification" },
   { SignTransactionRequestType, "com.celertech.marketmerchant.api.order.bitcoin.UpstreamBitcoinTransactionSigningProto$SignTransactionRequest" },
   { AccountDownstreamEventType, "com.celertech.staticdata.api.account.DownstreamAccountProto$AccountDownstreamEvent"},
   { AllSettlementAccountSnapshotDownstreamEventType, "com.blocksettle.private_bridge.accounts.DownstreamPrivateBridgeAccountProto$AllSettlementAccountSnapshotDownstreamEvent"},
   { AccountBulkUpdateDownstreamEventType, "com.blocksettle.private_bridge.accounts.DownstreamPrivateBridgeAccountProto$AccountBulkUpdateDownstreamEvent"},
   { AccoutBalanceUpdatedDownstreamEventType, "com.blocksettle.private_bridge.accounts.DownstreamPrivateBridgeAccountProto$AccoutBalanceUpdatedDownstreamEvent"},
   { ProcessedFxTradeCaptureReportDownstreamEventType, "com.celertech.clearing.api.tradecapturereport.processed.DownstreamProcessedTradeCaptureProto$ProcessedFxTradeCaptureReportDownstreamEvent" },
   { ProcessedTradeCaptureReportAckType, "com.celertech.clearing.api.tradecapturereport.processed.DownstreamProcessedTradeCaptureProto$ProcessedTradeCaptureReportAck" },
   { SubscribeToTerminalRequestType, "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$SubscribeToTerminalRequest" },
   { SubscribeToTerminalResponseType, "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$SubscribeToTerminalResponse" },
   { VerifiedAddressListUpdateEventType, "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$VerifiedAddressListUpdateEvent" },
   { SessionEndedEventType, "com.blocksettle.terminal.bsterminalapi.BSTerminalAPIProto$SessionEndedEvent" },
   { FindAssignedUserAccountsType, "com.celertech.staticdata.api.user.account.UpstreamUserAccountProto$FindAssignedUserAccounts"} ,
   { UserAccountDownstreamEventType, "com.celertech.staticdata.api.user.account.DownstreamUserAccountProto$UserAccountDownstreamEvent"} ,
   { FindAllSubLedgersByAccountType, "com.celertech.piggybank.api.subledger.UpstreamSubLedgerProto$FindAllSubLedgersByAccount"} ,
   { SubLedgerSnapshotDownstreamEventType, "com.celertech.piggybank.api.subledger.DownstreamSubLedgerProto$SubLedgerSnapshotDownstreamEvent"} ,
   { TransactionDownstreamEventType, "com.celertech.piggybank.api.generalledger.DownstreamGeneralLedgerProto$TransactionDownstreamEvent"},
   { VerifyXBTQuoteType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyXBTQuote" },
   { VerifyXBTQuoteRequestType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyXBTQuoteRequest" },
   { XBTTradeRequestType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$XBTTradeRequest" },
   { ReserveCashForXBTRequestType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$ReserveCashForXBTRequest" },
   { VerifyAuthenticationAddressResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyAuthenticationAddressResponse" },
   { VerifyXBTQuoteResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyXBTQuoteResponse" },
   { VerifyXBTQuoteRequestResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyXBTQuoteRequestResponse" },
   { XBTTradeResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$XBTTradeResponse" },
   { ReserveCashForXBTResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$ReserveCashForXBTResponse" },
   { FxTradeRequestType, "com.blocksettle.private_bridge.spotfx.UpstreamSpotFXProto$FxTradeRequest" },
   { FxTradeResponseType, "com.blocksettle.private_bridge.spotfx.DownstreamSpotFXProto$FxTradeResponse" },
   { VerifyColouredCoinQuoteType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyColouredCoinQuote" },
   { VerifyColouredCoinQuoteRequestType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyColouredCoinQuoteRequest" },
   { VerifyColouredCoinAcceptedQuoteType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$VerifyColouredCoinAcceptedQuote" },
   { ColouredCoinTradeRequestType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$ColouredCoinTradeRequest" },
   { VerifyColouredCoinQuoteResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyColouredCoinQuoteResponse" },
   { VerifyColouredCoinQuoteRequestResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyColouredCoinQuoteRequestResponse" },
   { VerifyColouredCoinAcceptedQuoteResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$VerifyColouredCoinAcceptedQuoteResponse" },
   { ColouredCoinTradeResponseType, "com.blocksettle.private_bridge.spotxbt.DownstreamSpotXBTProto$ColouredCoinTradeResponse" },
   { EndOfDayPriceReportType, "com.blocksettle.private_bridge.eod.UpstreamEoDProto$EndOfDayPriceReport"},
   { XBTTradeStatusRequestType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$XBTTradeStatusRequest"},
   { ColouredCoinTradeStatusRequestType, "com.blocksettle.private_bridge.spotxbt.UpstreamSpotXBTProto$ColouredCoinTradeStatusRequest"},
   { PersistenceExceptionType, "com.celertech.baseserver.api.exception.DownstreamExceptionProto$PersistenceException"},
   { MarketDataSubscriptionRequestType, "com.celertech.marketdata.api.price.UpstreamPriceProto$MarketDataSubscriptionRequest"},
   { MarketDataFullSnapshotDownstreamEventType, "com.celertech.marketdata.api.price.DownstreamPriceProto$MarketDataFullSnapshotDownstreamEvent"},
   { MarketDataRequestRejectDownstreamEventType, "com.celertech.marketdata.api.price.DownstreamPriceProto$MarketDataRequestRejectDownstreamEvent"},
   { MarketStatisticSnapshotDownstreamEventType, "com.celertech.marketdata.api.marketstatistic.DownstreamMarketStatisticProto$MarketStatisticSnapshotDownstreamEvent"},
   { MarketStatisticRequestType, "com.celertech.marketdata.api.marketstatistic.UpstreamMarketStatisticProto$MarketStatisticRequest"},
   { CreateSecurityDefinitionRequestType, "com.celertech.marketmerchant.api.securitydefinition.UpstreamSecurityDefinitionProto$CreateSecurityDefinition"},
   { CreateWarehouseConfigurationRequestType , "com.celertech.marketwarehouse.api.configuration.UpstreamWarehouseConfigurationProto$CreateWarehouseConfigurationRequest"},
   { WarehouseConfigurationDownstreamEventType, "com.celertech.marketwarehouse.api.configuration.DownstreamWarehouseConfigurationProto$WarehouseConfigurationDownstreamEvent"},
   { CreateSecurityListingRequestType, "com.celertech.staticdata.api.security.UpstreamSecurityProto$CreateSecurityListingRequest"},
   { SecurityListingDownstreamEventType, "com.celertech.staticdata.api.security.DownstreamSecurityProto$SecurityListingDownstreamEvent"},
   { FindAllSecurityDefinitionsType, "com.celertech.marketmerchant.api.securitydefinition.UpstreamSecurityDefinitionProto$FindAllSecurityDefinitions"},
   { SecurityDefinitionDownstreamEventType, "com.celertech.marketmerchant.api.securitydefinition.DownstreamSecurityDefinitionProto$SecurityDefinitionDownstreamEvent"},
   { FindAllSecurityListingsRequestType, "com.celertech.staticdata.api.security.UpstreamSecurityProto$FindAllSecurityListingsRequest"}
};

std::string GetMessageClass(CelerMessageType messageType)
{
   auto it = typeToName.find(messageType);
   if (it != typeToName.end()) {
      return it->second;
   }

   return "";
}

CelerMessageType GetMessageType(const std::string& fullClassName)
{
   auto it = nameToType.find(fullClassName);
   if (it != nameToType.end()) {
      return it->second;
   }
   return UndefinedType;
}

bool isValidMessageType(CelerMessageType messageType)
{
   return messageType >= CelerMessageTypeFirst &&
          messageType <= CelerMessageTypeLast &&
          messageType != UndefinedType;
}

};
