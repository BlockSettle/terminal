#include "ProtobufHeadlessUtils.h"
#include "CheckRecipSigner.h"


headless::SignTxRequest bs::signer::coreTxRequestToPb(const bs::core::wallet::TXSignRequest &txSignReq
   , bool keepDuplicatedRecipients)
{
   headless::SignTxRequest request;
   for (const auto &walletId : txSignReq.walletIds) {
      request.add_walletid(walletId);
   }
   request.set_keepduplicatedrecipients(keepDuplicatedRecipients);

   if (txSignReq.populateUTXOs) {
      request.set_populateutxos(true);
   }

   for (const auto &utxo : txSignReq.inputs) {
      request.add_inputs(utxo.serialize().toBinStr());
   }

   for (const auto &recip : txSignReq.recipients) {
      request.add_recipients(recip->getSerializedScript().toBinStr());
   }

   for (const auto &sortType : txSignReq.outSortOrder) {
      request.add_out_sort_order(static_cast<uint32_t>(sortType));
   }

   if (txSignReq.fee) {
      request.set_fee(txSignReq.fee);
   }

   if (txSignReq.RBF) {
      request.set_rbf(true);
   }

   if (!txSignReq.prevStates.empty()) {
      request.set_unsignedstate(txSignReq.serializeState().toBinStr());
   }

   if (txSignReq.change.value) {
      auto change = request.mutable_change();
      change->set_address(txSignReq.change.address.display());
      change->set_index(txSignReq.change.index);
      change->set_value(txSignReq.change.value);
   }

   return  request;
}

bs::core::wallet::TXSignRequest bs::signer::pbTxRequestToCore(const headless::SignTxRequest &request)
{
   //uint64_t inputVal = 0;
   bs::core::wallet::TXSignRequest txSignReq;

   for (int i = 0; i < request.walletid_size(); ++i) {
      txSignReq.walletIds.push_back(request.walletid(i));
   }
   for (int i = 0; i < request.inputs_size(); i++) {
      UTXO utxo;
      utxo.unserialize(request.inputs(i));
      if (utxo.isInitialized()) {
         txSignReq.inputs.push_back(utxo);
      }
   }

   uint64_t outputVal = 0;
   for (int i = 0; i < request.recipients_size(); i++) {
      BinaryData serialized = request.recipients(i);
      const auto recip = ScriptRecipient::deserialize(serialized);
      txSignReq.recipients.push_back(recip);
      outputVal += recip->getValue();
   }

   if (request.out_sort_order_size() == 3) {
      txSignReq.outSortOrder = { static_cast<bs::core::wallet::OutputOrderType>(request.out_sort_order(0))
         , static_cast<bs::core::wallet::OutputOrderType>(request.out_sort_order(1))
         , static_cast<bs::core::wallet::OutputOrderType>(request.out_sort_order(2)) };
   }

   if (request.has_change()) {
      txSignReq.change.address = bs::Address::fromAddressString(request.change().address());
      txSignReq.change.index = request.change().index();
      txSignReq.change.value = request.change().value();
   }

   int64_t value = outputVal;

   txSignReq.fee = request.fee();
   txSignReq.RBF = request.rbf();

   if (!request.unsignedstate().empty()) {
      const BinaryData prevState(request.unsignedstate());
      txSignReq.prevStates.push_back(prevState);
      if (!value) {
         bs::CheckRecipSigner signer(prevState);
         value = signer.spendValue();
         if (txSignReq.change.value) {
            value -= txSignReq.change.value;
         }
      }
   }

   txSignReq.populateUTXOs = request.populateutxos();

   return txSignReq;
}
