#ifndef TRADES_UTILS_H
#define TRADES_UTILS_H

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include "Address.h"
#include "BinaryData.h"
#include "CoreWallet.h"
#include "TxClasses.h"
#include "XBTAmount.h"

class ArmoryConnection;
class AuthAddressManager;
class SignContainer;
struct OtcClientDeal;

namespace bs {
   class Address;
   class UtxoReservation;
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }
   namespace sync {
      class Wallet;
      class WalletsManager;
      namespace hd {
         class SettlementLeaf;
      }
   }


   namespace tradeutils {
      // Request getSpendableTxOutList for every wallet in wallets, merge results (keeping requested order) and call callback.
      // If request failed for at least one wallet callback would not be called.
      bool getSpendableTxOutList(const std::vector<std::shared_ptr<bs::sync::Wallet>> &wallets
         // maps UTXO to the wallet used
         , const std::function<void(const std::map<UTXO, std::string> &)> &);

      struct Args
      {
         bs::XBTAmount amount;
         BinaryData settlementId;

         bs::Address ourAuthAddress;
         BinaryData cpAuthPubKey;

         std::shared_ptr<bs::sync::WalletsManager> walletsMgr;
         std::shared_ptr<ArmoryConnection> armory;
         std::shared_ptr<SignContainer> signContainer;
      };

      struct PayinArgs : public Args
      {
         // Set if createPayin is used with manual UTXO selection
         std::vector<UTXO> fixedInputs;

         // Could be set to native, nested or both.
         // Must be from the same hd wallet.
         // First wallet used to send XBT change if needed.
         std::vector<std::shared_ptr<bs::sync::Wallet>> inputXbtWallets;

         // If set, automatic UTXO selection filters reserverd inputs.
         // Fixed inputs are not filtered.
         bs::UtxoReservation *utxoReservation{};

         // walletId used for UTXO filtering
         std::string utxoReservationWalletId;
      };

      struct PayoutArgs : public Args
      {
         BinaryData payinTxId;

         // If set this addr will be used for output
         // If not set new ext address would be requested from outputXbtWallet for output
         bs::Address recvAddr;

         std::shared_ptr<bs::sync::Wallet> outputXbtWallet;
      };

      struct Result
      {
         bool success{};
         std::string errorMsg;

         bs::Address settlementAddr;
         bs::core::wallet::TXSignRequest signRequest;

         static Result error(std::string msg);
      };

      struct PayinResult : public Result
      {
         std::map<bs::Address, BinaryData> preimageData;

         BinaryData     payinHash;

         static PayinResult error(std::string msg);
      };

      using PayoutResult = Result;

      using PayinResultCb = std::function<void(bs::tradeutils::PayinResult result)>;
      using PayoutResultCb = std::function<void(bs::tradeutils::PayoutResult result)>;

      unsigned feeTargetBlockCount();

      uint64_t getEstimatedFeeFor(UTXO input, const bs::Address &recvAddr
         , float feePerByte, unsigned int topBlock);

      uint64_t estimatePayinFeeWithoutChange(const std::vector<UTXO> &inputs, float feePerByte);

      UTXO getInputFromTX(const bs::Address &, const BinaryData &payinHash
         , const bs::XBTAmount& amount);
      bs::core::wallet::TXSignRequest createPayoutTXRequest(UTXO
         , const bs::Address &recvAddr, float feePerByte, unsigned int topBlock);

      // Callback is called from background thread
      void createPayin(PayinArgs args, PayinResultCb cb);

      // Callback is called from background thread
      void createPayout(PayoutArgs args, PayoutResultCb cb);

      struct PayoutVerifyArgs
      {
         BinaryData signedTx;
         bs::Address settlAddr;
         BinaryData usedPayinHash;
         bs::XBTAmount amount;
      };

      struct PayoutVerifyResult
      {
         bool success{};
         std::string errorMsg;
      };

      PayoutVerifyResult verifySignedPayout(PayoutVerifyArgs args);

   } // namespace tradeutils
} // namespace bs

#endif
