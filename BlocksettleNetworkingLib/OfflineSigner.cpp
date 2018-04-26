#include "OfflineSigner.h"
#include <QDateTime>
#include <QFile>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "Address.h"
#include "MetaData.h"
#include "signer.pb.h"

using namespace Blocksettle;

OfflineSigner::OfflineSigner(const std::shared_ptr<spdlog::logger> &logger, const QString &dir)
   : SignContainer(logger, SignContainer::OpMode::Offline), targetDir_(dir)
{ }

bool OfflineSigner::Start()
{
   emit ready();
   return true;
}

SignContainer::RequestId OfflineSigner::SignTXRequest(const bs::wallet::TXSignRequest &txSignReq, bool, TXSignMode, const PasswordType&)
{
   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainer] Invalid TXSignRequest");
      return 0;
   }

   Storage::Signer::TXRequest request;
   request.set_walletid(txSignReq.walletId);

   for (const auto &utxo : txSignReq.inputs) {
      auto input = request.add_inputs();
      input->set_utxo(utxo.serialize().toBinStr());
      const auto addr = bs::Address::fromUTXO(utxo);
      input->mutable_address()->set_address(addr.display<std::string>());
      if (txSignReq.wallet) {
         input->mutable_address()->set_index(txSignReq.wallet->GetAddressIndex(addr));
      }
   }

   for (const auto &recip : txSignReq.recipients) {
      request.add_recipients(recip->getSerializedScript().toBinStr());
   }

   if (txSignReq.fee) {
      request.set_fee(txSignReq.fee);
   }
   if (txSignReq.RBF) {
      request.set_rbf(true);
   }

   if (txSignReq.change.value) {
      auto change = request.mutable_change();
      change->mutable_address()->set_address(txSignReq.change.address.display<std::string>());
      change->mutable_address()->set_index(txSignReq.change.index);
      change->set_value(txSignReq.change.value);
   }

   if (!txSignReq.comment.empty()) {
      request.set_comment(txSignReq.comment);
   }

   Storage::Signer::File fileContainer;
   auto container = fileContainer.add_payload();
   container->set_type(Storage::Signer::RequestFileType);
   container->set_data(request.SerializeAsString());

   const auto timestamp = std::to_string(QDateTime::currentDateTime().toSecsSinceEpoch());
   const std::string fileName = targetDir_.toStdString() + "/" + txSignReq.walletId + "_" + timestamp + ".bin";

   const auto reqId = seqId_++;
   QFile f(QString::fromStdString(fileName));
   if (f.exists()) {
      QTimer::singleShot(0, [this, reqId, fileName] {
         emit TXSigned(reqId, {}, "request file " + fileName + " already exists");
      });
      return reqId;
   }
   if (!f.open(QIODevice::WriteOnly)) {
      QTimer::singleShot(0, [this, reqId, fileName] {
         emit TXSigned(reqId, {}, "failed to open " + fileName + " for writing");
      });
      return reqId;
   }

   const auto data = QByteArray::fromStdString(fileContainer.SerializeAsString());
   if (f.write(data) != data.size()) {
      QTimer::singleShot(0, [this, reqId, fileName] { emit TXSigned(reqId, {}, "failed to write to " + fileName); });
      return reqId;
   }
   f.close();

   QTimer::singleShot(0, [this, reqId, fileName] { emit TXSigned(reqId, fileName, {}); });
   return reqId;
}


std::vector<bs::wallet::TXSignRequest> ParseOfflineTXFile(const std::string &data)
{
   Storage::Signer::File fileContainer;
   if (!fileContainer.ParseFromString(data)) {
      return {};
   }
   std::vector<bs::wallet::TXSignRequest> result;
   for (int i = 0; i < fileContainer.payload_size(); i++) {
      const auto container = fileContainer.payload(i);
      bs::wallet::TXSignRequest txReq;
      if (container.type() == Storage::Signer::RequestFileType) {
         Storage::Signer::TXRequest tx;
         if (!tx.ParseFromString(container.data())) {
            continue;
         }
         txReq.walletId = tx.walletid();
         txReq.fee = tx.fee();
         txReq.RBF = tx.rbf();
         txReq.comment = tx.comment();

         for (int i = 0; i < tx.inputs_size(); i++) {
            UTXO utxo;
            utxo.unserialize(tx.inputs(i).utxo());
            if (utxo.isInitialized()) {
               txReq.inputs.push_back(utxo);
            }
         }

         for (int i = 0; i < tx.recipients_size(); i++) {
            BinaryData serialized = tx.recipients(i);
            const auto recip = ScriptRecipient::deserialize(serialized);
            txReq.recipients.push_back(recip);
         }

         if (tx.has_change()) {
            txReq.change.value = tx.change().value();
            txReq.change.address = tx.change().address().address();
            txReq.change.index = tx.change().address().index();
         }
      }
      else if (container.type() == Storage::Signer::SignedTXFileType) {
         Storage::Signer::SignedTX tx;
         if (!tx.ParseFromString(container.data())) {
            continue;
         }
         txReq.prevStates.push_back(tx.transaction());
         txReq.comment = tx.comment();
      }
      else {
         continue;
      }
      result.push_back(txReq);
   }
   return result;
}
