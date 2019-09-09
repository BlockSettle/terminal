#include "TXInfo.h"
#include "QWalletInfo.h"

using namespace bs::wallet;
using namespace Blocksettle::Communication;

void TXInfo::init()
{
   txId_ = QString::fromStdString(txReq_.serializeState().toBinStr());
}

void TXInfo::setTxId(const QString &txId)
{
   txId_ = txId;
   emit dataChanged();
}

bs::core::wallet::TXSignRequest TXInfo::getCoreSignTxRequest(const signer::SignTxRequest &req)
{
   bs::core::wallet::TXSignRequest txReq;
   txReq.walletIds = { req.wallet_id() };
   for (int i = 0; i < req.inputs_size(); ++i) {
      UTXO utxo;
      utxo.unserialize(req.inputs(i));
      txReq.inputs.emplace_back(std::move(utxo));
   }
   for (int i = 0; i < req.recipients_size(); ++i) {
      const BinaryData bd(req.recipients(i));
      txReq.recipients.push_back(ScriptRecipient::deserialize(bd));
   }
   txReq.fee = req.fee();
   txReq.RBF = req.rbf();
   if (req.has_change()) {
      txReq.change.address = req.change().address();
      txReq.change.index = req.change().index();
      txReq.change.value = req.change().value();
   }
   return  txReq;
}

QStringList TXInfo::inputs() const
{
   QStringList result;
   for (const auto &input : txReq_.inputs) {
      result.push_back(QString::fromStdString(bs::Address::fromUTXO(input).display()));
   }
   return result;
}

QStringList TXInfo::recipients() const
{
   QStringList result;
   for (const auto &recip : txReq_.recipients) {
      result.push_back(QString::fromStdString(bs::Address::fromRecipient(recip).display()));
   }
   return result;
}

double TXInfo::amount() const
{
   uint64_t result = 0;
   for (const auto &recip : txReq_.recipients) {
      result += recip->getValue();
   }
   return result / BTCNumericTypes::BalanceDivider;
}

double TXInfo::inputAmount() const
{
   uint64_t result = 0;
   for (const auto &utxo: txReq_.inputs) {
      result += utxo.getValue();
   }
   return result / BTCNumericTypes::BalanceDivider;
}
