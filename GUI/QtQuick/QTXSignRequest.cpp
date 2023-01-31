/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QTXSignRequest.h"
#include "Address.h"
#include "BTCNumericTypes.h"


QTXSignRequest::QTXSignRequest(QObject* parent)
   : QObject(parent)
{}

void QTXSignRequest::setTxSignReq(const bs::core::wallet::TXSignRequest& txReq)
{
   txReq_ = txReq;
   emit txSignReqChanged();
}

void QTXSignRequest::setError(const QString& err)
{
   error_ = err;
   emit error();
}

QStringList QTXSignRequest::outputAddresses() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   QStringList result;
   for (const auto& recip : txReq_.getRecipients([](const bs::Address&) { return true; })) {
      try {
         const auto& addr = bs::Address::fromRecipient(recip);
         result.append(QString::fromStdString(addr.display()));
      }
      catch (const std::exception& e) {
         result.append(QLatin1String("error: ") + QLatin1String(e.what()));
      }
   }
   return result;
}

QString QTXSignRequest::outputAmount() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   return QString::number(txReq_.amountReceived([changeAddr = txReq_.change.address]
      (const bs::Address& addr) { return (addr != changeAddr); }) / BTCNumericTypes::BalanceDivider
      , 'f', 8);
}

QString QTXSignRequest::inputAmount() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   return QString::number(txReq_.armorySigner_.getTotalInputsValue() / BTCNumericTypes::BalanceDivider
      , 'f', 8);
}

QString QTXSignRequest::returnAmount() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   return QString::number(txReq_.changeAmount() / BTCNumericTypes::BalanceDivider
      , 'f', 8);
}

QString QTXSignRequest::fee() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   return QString::number(txReq_.getFee() / BTCNumericTypes::BalanceDivider
      , 'f', 8);
}

quint32 QTXSignRequest::txSize() const
{
   if (!txReq_.isValid()) {
      return 0;
   }
   return txReq_.estimateTxVirtSize();
}

QString QTXSignRequest::feePerByte() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   return QString::number(txReq_.getFee() / (double)txReq_.estimateTxVirtSize(), 'f', 1);
}
