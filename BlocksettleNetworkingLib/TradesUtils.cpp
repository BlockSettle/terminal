#include "TradesUtils.h"

#include <spdlog/spdlog.h>

#include "CoinSelection.h"
#include "SettlementMonitor.h"
#include "UtxoReservation.h"
#include "Wallets/SyncHDGroup.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"

namespace {

   std::shared_ptr<bs::sync::hd::SettlementLeaf> findSettlementLeaf(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr, const bs::Address &ourAuthAddress)
   {
      auto wallet = walletsMgr->getPrimaryWallet();
      if (!wallet) {
         return nullptr;
      }

      auto group = std::dynamic_pointer_cast<bs::sync::hd::SettlementGroup>(wallet->getGroup(bs::hd::BlockSettle_Settlement));
      if (!group) {
         return nullptr;
      }

      return group->getLeaf(ourAuthAddress);
   }

} // namespace

bs::tradeutils::Result bs::tradeutils::Result::error(std::string msg)
{
   bs::tradeutils::Result result;
   result.errorMsg = std::move(msg);
   return result;
}

bs::tradeutils::PayinResult bs::tradeutils::PayinResult::error(std::string msg)
{
   bs::tradeutils::PayinResult result;
   result.errorMsg = std::move(msg);
   return result;
}

unsigned bs::tradeutils::feeTargetBlockCount()
{
   return 2;
}

uint64_t bs::tradeutils::estimatePayinFeeWithoutChange(const std::vector<UTXO> &inputs, float feePerByte)
{
   // add workaround for computeSizeAndFee (it can't compute exact v-size before signing,
   // sometimes causing "fee not met" error for 1 sat/byte)
   if (feePerByte >= 1.0f && feePerByte < 1.01f) {
      feePerByte = 1.01f;
   }

   std::map<unsigned, std::shared_ptr<ScriptRecipient>> recipientsMap;
   // Use some fake settlement address as the only recipient
   BinaryData prefixed;
   prefixed.append(AddressEntry::getPrefixByte(AddressEntryType_P2WSH));
   prefixed.append(CryptoPRNG::generateRandom(32));
   auto recipient = bs::Address::fromHash(prefixed);
   // Select some random amount
   recipientsMap[0] = recipient.getRecipient(bs::XBTAmount{ uint64_t{1000} });

   auto inputsCopy = bs::Address::decorateUTXOsCopy(inputs);
   PaymentStruct payment(recipientsMap, 0, feePerByte, 0);
   uint64_t result = bs::Address::getFeeForMaxVal(inputsCopy, payment.size_, feePerByte);
   return result;
}

void bs::tradeutils::createPayin(bs::tradeutils::PayinArgs args, bs::tradeutils::PayinResultCb cb)
{
   auto leaf = findSettlementLeaf(args.walletsMgr, args.ourAuthAddress);
   if (!leaf) {
      cb(PayinResult::error("can't find settlement leaf"));
      return;
   }

   if (args.inputXbtWallets.empty()) {
      cb(PayinResult::error("XBT wallets not set"));
      return;
   }

   leaf->setSettlementID(args.settlementId, [args, cb](bool result)
   {
      if (!result) {
         cb(PayinResult::error("setSettlementID failed"));
         return;
      }

      auto cbFee = [args, cb](float fee) {
         auto feePerByte = ArmoryConnection::toFeePerByte(fee);
         if (feePerByte < 1.0f) {
            cb(PayinResult::error("invalid feePerByte"));
            return;
         }

         auto primaryHdWallet = args.walletsMgr->getPrimaryWallet();
         if (!primaryHdWallet) {
            cb(PayinResult::error("can't find primary wallet"));
            return;
         }

         const auto &xbtWallet = args.inputXbtWallets.front();

         auto cbSettlAddr = [args, cb, feePerByte, xbtWallet](const bs::Address &settlAddr)
         {
            if (settlAddr.isNull()) {
               cb(PayinResult::error("invalid settl addr"));
               return;
            }

            auto inputsCb = [args, cb, settlAddr, feePerByte, xbtWallet](const std::vector<UTXO> &utxosOrig) {
               auto utxos = bs::Address::decorateUTXOsCopy(utxosOrig);

               std::map<unsigned, std::shared_ptr<ScriptRecipient>> recipientsMap;
               auto recipient = settlAddr.getRecipient(args.amount);
               recipientsMap.emplace(0, recipient);
               auto payment = PaymentStruct(recipientsMap, 0, feePerByte, 0);

               auto coinSelection = CoinSelection(nullptr, {}, args.amount.GetValue(), args.armory->topBlock());

               try {
                  auto selection = coinSelection.getUtxoSelectionForRecipients(payment, utxos);
                  auto selectedInputs = selection.utxoVec_;
                  auto fee = selection.fee_;

                  const auto cbPreimage = [args, cb, settlAddr, xbtWallet, selectedInputs, recipient, fee]
                     (const std::map<bs::Address, BinaryData> &preimages)
                  {
                     const auto resolver = bs::sync::WalletsManager::getPublicResolver(preimages);

                     auto changeCb = [args, cb, settlAddr, resolver, xbtWallet, selectedInputs, recipient, fee](const bs::Address &changeAddr)
                     {
                        PayinResult result;
                        result.settlementAddr = settlAddr;
                        result.success = true;

                        auto recipients = std::vector<std::shared_ptr<ScriptRecipient>>(1, recipient);
                        try {
                           result.signRequest = xbtWallet->createTXRequest(selectedInputs, recipients, fee, false, changeAddr);

                           result.payinTxId = result.signRequest.txId(resolver);

                        } catch (const std::exception &e) {
                           cb(PayinResult::error(fmt::format("creating paying request failed: {}", e.what())));
                           return;
                        }

                        if (!result.signRequest.isValid()) {
                           cb(PayinResult::error("invalid pay-in transaction"));
                           return;
                        }

                        cb(std::move(result));
                     };

                     xbtWallet->getNewIntAddress(changeCb);
                  };

                  const auto addrMapping = args.walletsMgr->getAddressToWalletsMapping(selectedInputs);
                  args.signContainer->getAddressPreimage(addrMapping, cbPreimage);

               } catch (const std::exception &e) {
                  cb(PayinResult::error(fmt::format("unexpected exception: {}", e.what())));
                  return;
               }
            };

            if (args.fixedInputs.empty()) {
               auto inputsCbWrap = [args, cb, inputsCb](std::vector<UTXO> utxos) {
                  if (args.utxoReservation) {
                     // Ignore filter return value as it fails if there were no reservations before
                     args.utxoReservation->filter(args.utxoReservationWalletId, utxos);
                  }
                  inputsCb(utxos);
               };
               bs::sync::Wallet::getSpendableTxOutList(args.inputXbtWallets, inputsCbWrap);
            } else {
               inputsCb(args.fixedInputs);
            }
         };

         const bool myKeyFirst = false;
         primaryHdWallet->getSettlementPayinAddress(args.settlementId, args.cpAuthPubKey, cbSettlAddr, myKeyFirst);
      };

      args.armory->estimateFee(feeTargetBlockCount(), cbFee);
   });
}

void bs::tradeutils::createPayout(bs::tradeutils::PayoutArgs args, bs::tradeutils::PayoutResultCb cb)
{
   auto leaf = findSettlementLeaf(args.walletsMgr, args.ourAuthAddress);
   if (!leaf) {
      cb(PayoutResult::error("can't find settlement leaf"));
      return;
   }

   leaf->setSettlementID(args.settlementId, [args, cb](bool result)
   {
      if (!result) {
         cb(PayoutResult::error("setSettlementID failed"));
         return;
      }

      auto cbFee = [args, cb](float fee) {
         auto feePerByte = ArmoryConnection::toFeePerByte(fee);
         if (feePerByte < 1.0f) {
            cb(PayoutResult::error("invalid feePerByte"));
            return;
         }

         auto primaryHdWallet = args.walletsMgr->getPrimaryWallet();
         if (!primaryHdWallet) {
            cb(PayoutResult::error("can't find primary wallet"));
            return;
         }

         auto cbSettlAddr = [args, cb, feePerByte](const bs::Address &settlAddr) {
            auto recvAddrCb = [args, cb, feePerByte, settlAddr](const bs::Address &recvAddr) {
               if (settlAddr.isNull()) {
                  cb(PayoutResult::error("invalid settl addr"));
                  return;
               }

               auto payinUTXO = bs::SettlementMonitor::getInputFromTX(settlAddr, args.payinTxId, args.amount);

               PayoutResult result;
               result.success = true;
               result.settlementAddr = settlAddr;
               result.signRequest = bs::SettlementMonitor::createPayoutTXRequest(
                  payinUTXO, recvAddr, feePerByte, args.armory->topBlock());
               cb(std::move(result));
            };

            if (!args.recvAddr.isNull()) {
               recvAddrCb(args.recvAddr);
            } else {
               args.outputXbtWallet->getNewExtAddress(recvAddrCb);
            }
         };

         const bool myKeyFirst = true;
         primaryHdWallet->getSettlementPayinAddress(args.settlementId, args.cpAuthPubKey, cbSettlAddr, myKeyFirst);
      };

      args.armory->estimateFee(feeTargetBlockCount(), cbFee);
   });
}

bs::tradeutils::PayoutVerifyResult bs::tradeutils::verifySignedPayout(bs::tradeutils::PayoutVerifyArgs args)
{
   PayoutVerifyResult result;

   try {
      Tx tx(args.signedTx);

      auto txdata = tx.serialize();
      auto bctx = BCTX::parse(txdata);

      auto utxo = bs::SettlementMonitor::getInputFromTX(args.settlAddr, args.usedPayinHash, args.amount);

      std::map<BinaryData, std::map<unsigned, UTXO>> utxoMap;
      utxoMap[utxo.getTxHash()][0] = utxo;

      TransactionVerifier tsv(*bctx, utxoMap);

      auto tsvFlags = tsv.getFlags();
      tsvFlags |= SCRIPT_VERIFY_P2SH_SHA256 | SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_SEGWIT;
      tsv.setFlags(tsvFlags);

      auto verifierState = tsv.evaluateState();

      auto inputState = verifierState.getSignedStateForInput(0);

      auto signatureCount = inputState.getSigCount();

      if (signatureCount != 1) {
         result.errorMsg = fmt::format("signature count: {}", signatureCount);
         return result;
      }

      result.success = true;
      return result;

   } catch (const std::exception &e) {
      result.errorMsg = fmt::format("failed: {}", e.what());
      return result;
   }
}
