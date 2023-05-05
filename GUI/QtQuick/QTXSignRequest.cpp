/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QTXSignRequest.h"
#include <spdlog/spdlog.h>
#include "Address.h"
#include "BTCNumericTypes.h"


QTXSignRequest::QTXSignRequest(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QObject(parent), logger_(logger)
{}

void QTXSignRequest::setTxSignReq(const bs::core::wallet::TXSignRequest& txReq)
{
   txReq_ = txReq;

   if (outputsModel_) {
      outputsModel_->clearOutputs();
   }
   else {
      outputsModel_ = new TxOutputsModel(logger_, this, true);
   }
   for (const auto& recip : txReq_.getRecipients([](const bs::Address&) { return true; })) {
      try {
         const auto& addr = bs::Address::fromRecipient(recip);
         outputsModel_->addOutput(QString::fromStdString(addr.display())
            , recip->getValue() / BTCNumericTypes::BalanceDivider
            , addr == txReq_.change.address);
      }
      catch (const std::exception& ) {}
   }
   emit txSignReqChanged();

   std::vector<UTXO> inputs;
   inputs.reserve(txReq.armorySigner_.getTxInCount());
   for (unsigned int i = 0; i < txReq.armorySigner_.getTxInCount(); ++i) {
      const auto& spender = txReq.armorySigner_.getSpender(i);
      inputs.push_back(spender->getUtxo());
   }
   setInputs(inputs);
}

void QTXSignRequest::setError(const QString& err)
{
   error_ = err;
   emit errorSet();
}

void QTXSignRequest::setInputs(const std::vector<UTXO>& utxos)
{
   logger_->debug("[{}] {} UTXO[s]", __func__, utxos.size());
   if (inputsModel_) {
      inputsModel_->clear();
      inputsModel_->addUTXOs(utxos);
      emit txSignReqChanged();
   }
   else {
      inputsModel_ = new TxInputsModel(logger_, nullptr, this);
      inputsModel_->addUTXOs(utxos);
      emit txSignReqChanged();
   }
}

QStringList QTXSignRequest::outputAddresses() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   QStringList result;
   for (const auto& recip : txReq_.getRecipients([changeAddr = txReq_.change.address](const bs::Address& addr)
   { return addr != changeAddr;  })) {
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

QStringList QTXSignRequest::outputAmounts() const
{
   if (!txReq_.isValid()) {
      return {};
   }
   QStringList result;
   for (const auto& recip : txReq_.getRecipients([changeAddr = txReq_.change.address](const bs::Address& addr) { return addr != changeAddr; })) {
       try {
           const auto& recipAddr = bs::Address::fromRecipient(recip);
           result.append(
               QString::number(txReq_.amountReceived([recipAddr]
               (const bs::Address& addr) { return (addr == recipAddr); }) / BTCNumericTypes::BalanceDivider, 'f', 8));
       }
       catch (const std::exception& e) {
           result.append(QLatin1String("error: ") + QLatin1String(e.what()));
       }
   }
   return result;
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

bool QTXSignRequest::isWatchingOnly() const
{
   return isWatchingOnly_;
}

void QTXSignRequest::setWatchingOnly(bool watchingOnly)
{
   isWatchingOnly_ = watchingOnly;
   emit txSignReqChanged();
}

QString QTXSignRequest::comment() const
{
    if (!txReq_.isValid()) {
        return {};
    }
    return QString::fromStdString(txReq_.comment);
}
