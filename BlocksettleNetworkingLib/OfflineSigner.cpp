#include "OfflineSigner.h"
#include "Address.h"
#include "signer.pb.h"

using namespace Blocksettle;

std::vector<bs::core::wallet::TXSignRequest> ParseOfflineTXFile(const std::string &data)
{
   Storage::Signer::File fileContainer;
   if (!fileContainer.ParseFromString(data)) {
      return {};
   }
   std::vector<bs::core::wallet::TXSignRequest> result;
   for (int i = 0; i < fileContainer.payload_size(); i++) {
      const auto container = fileContainer.payload(i);
      bs::core::wallet::TXSignRequest txReq;
      if (container.type() == Storage::Signer::RequestFileType) {
         Storage::Signer::TXRequest tx;
         if (!tx.ParseFromString(container.data())) {
            continue;
         }
         for (int i = 0; i < tx.walletid_size(); ++i) {
            txReq.walletIds.push_back(tx.walletid(i));
         }
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
