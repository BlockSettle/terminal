/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QDate>
#include <spdlog/spdlog.h>
#include "CelerSubmitQuoteNotifSequence.h"
#include "UpstreamQuoteProto.pb.h"

using namespace bs::network;
using namespace com::celertech::marketmerchant::api;


CelerSubmitQuoteNotifSequence::CelerSubmitQuoteNotifSequence(const std::string& accountName
   , const QuoteNotification& qn, const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerSubmitQuoteNotifSequence",
      {
         { false, nullptr, &CelerSubmitQuoteNotifSequence::submitQuoteNotif }
      })
 , accountName_(accountName)
 , qn_(qn)
 , logger_(logger)
{}


bool CelerSubmitQuoteNotifSequence::FinishSequence()
{
   return true;
}

CelerMessage CelerSubmitQuoteNotifSequence::submitQuoteNotif()
{
   quote::QuoteNotification request;

   request.set_requestorsessionkey(qn_.sessionToken.substr(0, qn_.sessionToken.find(':')));
   request.set_requestorsessiontoken(qn_.sessionToken);
   request.set_quoterequestid(qn_.quoteRequestId);
   request.set_assettype(Asset::toCeler(qn_.assetType));
   request.set_producttype(Asset::toCelerProductType(qn_.assetType));
   request.set_securitycode(qn_.security);
   request.set_securityid(qn_.security);
   request.set_side(Side::toCeler(qn_.side));

   if (!qn_.authKey.empty()) {
      request.set_dealerauthenticationaddress(qn_.authKey);
   }

   if (qn_.assetType == Asset::PrivateMarket) {
      request.set_cointransactioninput(qn_.transactionData);
      request.set_receiptaddress(qn_.receiptAddress);
   }
   else if (qn_.assetType == Asset::SpotXBT) {
      if (!qn_.transactionData.empty()) {
         request.set_dealertransaction(qn_.transactionData);
      }
   }

   if (!qFuzzyIsNull(qn_.bidPx)) {
      request.set_bidpx(qn_.bidPx);
      request.set_bidspotpx(qn_.bidPx);
   }
   if (!qFuzzyIsNull(qn_.offerPx)) {
      request.set_offerpx(qn_.offerPx);
      request.set_offerspotpx(qn_.offerPx);
   }

   request.set_quotevalidityinsecs(qn_.validityInS);
   request.set_accountbookon(accountName_);

   if (!qFuzzyIsNull(qn_.bidContraQty)) {
      request.set_bidcontraqty(qn_.bidContraQty);
   }
   if (!qFuzzyIsNull(qn_.offerContraQty)) {
      request.set_offercontraqty(qn_.offerContraQty);
   }

   quote::QuoteNotification::LegQuoteGroup *group = request.add_legquotegroup();
   if (!qFuzzyIsNull(qn_.bidFwdPts)) {
      group->set_bidforwardpoints(qn_.bidFwdPts);
   }
   if (!qFuzzyIsNull(qn_.offerFwdPts)) {
      group->set_offerforwardpoints(qn_.offerFwdPts);
   }
   if (!qFuzzyIsNull(qn_.bidSz)) {
      group->set_bidsize(qn_.bidSz);
   }
   if (!qFuzzyIsNull(qn_.offerSz)) {
      group->set_offersize(qn_.offerSz);
   }
   group->set_currency(qn_.product);
   group->set_settlementdate(QDate::currentDate().toString(Qt::ISODate).toStdString());

   CelerMessage message;
   message.messageType = CelerAPI::QuoteNotificationType;
   message.messageData = request.SerializeAsString();

   logger_->debug("[CelerSubmitQuoteNotifSequence::submitQuoteNotif] {}", request.DebugString());

   return message;
}
