/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerSignTxSequence.h"

#include "bitcoin/UpstreamBitcoinTransactionSigningProto.pb.h"

#include <QDate>

#include <spdlog/spdlog.h>

using namespace bs::network;
using namespace com::celertech::marketmerchant::api::order::bitcoin;

CelerSignTxSequence::CelerSignTxSequence(const QString &orderId, const std::string &txData, const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerSignTxSequence", {
         { false, nullptr, &CelerSignTxSequence::send }
      })
   , orderId_(orderId)
   , txData_(txData)
   , logger_(logger)
{}


CelerMessage CelerSignTxSequence::send()
{
   SignTransactionRequest request;

   request.set_orderid(orderId_.toStdString());
   request.set_signedtransaction(txData_);

   CelerMessage message;
   message.messageType = CelerAPI::SignTransactionRequestType;
   message.messageData = request.SerializeAsString();

   logger_->debug("SignTransaction: {}", request.DebugString());

   return message;
}
