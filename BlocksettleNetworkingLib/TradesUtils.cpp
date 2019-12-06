/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TradesUtils.h"

#include <spdlog/spdlog.h>

#include "CoinSelection.h"
#include "FastLock.h"
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

bool bs::tradeutils::getSpendableTxOutList(const std::vector<std::shared_ptr<bs::sync::Wallet>> &wallets
   , const std::function<void(const std::map<UTXO, std::string> &)> &cb)
{
   if (wallets.empty()) {
      cb({});
      return true;
   }
   struct Result
   {
      std::map<std::string, std::vector<UTXO>> utxosMap;
      std::function<void(const std::map<UTXO, std::string> &)> cb;
      std::atomic_flag lockFlag = ATOMIC_FLAG_INIT;
   };
   auto result = std::make_shared<Result>();
   result->cb = std::move(cb);

   for (const auto &wallet : wallets) {
      auto cbWrap = [result, size = wallets.size(), walletId = wallet->walletId()]
      (std::vector<UTXO> utxos)
      {
         FastLock lock(result->lockFlag);
         result->utxosMap.emplace(walletId, std::move(utxos));
         if (result->utxosMap.size() != size) {
            return;
         }

         std::map<UTXO, std::string> utxosAll;

         for (auto &item : result->utxosMap) {
            for (const auto &utxo : item.second) {
               utxosAll[utxo] = item.first;
            }
         }
         result->cb(utxosAll);
      };

      // If request for some wallet failed resulted callback won't be called.
      if (!wallet->getSpendableTxOutList(cbWrap, UINT64_MAX)) {
         return false;
      }
   }
   return true;
}

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

                  auto changeCb = [args, selectedInputs, fee, settlAddr, xbtWallet, recipient, cb](const bs::Address &changeAddr)
                  {
                     std::vector<UTXO> p2shInputs;

                     for ( const auto& input : selectedInputs) {
                        const auto scrType = BtcUtils::getTxOutScriptType(input.getScript());

                        if (scrType == TXOUT_SCRIPT_P2SH) {
                           p2shInputs.push_back(input);
                        }
                     }

                     const auto cbPreimage = [args, settlAddr, cb, recipient, xbtWallet, selectedInputs, fee, changeAddr]
                        (const std::map<bs::Address, BinaryData> &preimages)
                     {
                        PayinResult result;
                        result.settlementAddr = settlAddr;
                        result.success = true;

                        const auto resolver = bs::sync::WalletsManager::getPublicResolver(preimages);

                        auto recipients = std::vector<std::shared_ptr<ScriptRecipient>>(1, recipient);
                        try {
                           result.signRequest = xbtWallet->createTXRequest(selectedInputs, recipients, fee, false, changeAddr);
                           result.preimageData = preimages;
                           result.payinHash = result.signRequest.txId(resolver);

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

                     if (p2shInputs.empty()) {
                        cbPreimage({});
                     } else {
                        const auto addrMapping = args.walletsMgr->getAddressToWalletsMapping(p2shInputs);
                        args.signContainer->getAddressPreimage(addrMapping, cbPreimage);
                     }
                  };

                  xbtWallet->getNewIntAddress(changeCb);
               } catch (const std::exception &e) {
                  cb(PayinResult::error(fmt::format("unexpected exception: {}", e.what())));
                  return;
               }
            };

            if (args.fixedInputs.empty()) {
               auto inputsCbWrap = [args, cb, inputsCb](std::map<UTXO, std::string> inputs) {
                  std::vector<UTXO> utxos;
                  utxos.reserve(inputs.size());
                  for (const auto &input : inputs) {
                     utxos.emplace_back(std::move(input.first));
                  }
                  if (args.utxoReservation) {
                     // Ignore filter return value as it fails if there were no reservations before
                     args.utxoReservation->filter(args.utxoReservationWalletId, utxos);
                  }
                  inputsCb(utxos);
               };
               getSpendableTxOutList(args.inputXbtWallets, inputsCbWrap);
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

uint64_t bs::tradeutils::getEstimatedFeeFor(UTXO input, const bs::Address &recvAddr
   , float feePerByte, unsigned int topBlock)
{
   if (!input.isInitialized()) {
      return 0;
   }
   const auto inputAmount = input.getValue();
   if (input.txinRedeemSizeBytes_ == UINT32_MAX) {
      const auto scrAddr = bs::Address::fromHash(input.getRecipientScrAddr());
      input.txinRedeemSizeBytes_ = (unsigned int)scrAddr.getInputSize();
   }
   CoinSelection coinSelection([&input](uint64_t) -> std::vector<UTXO> { return { input }; }
   , std::vector<AddressBookEntry>{}, inputAmount, topBlock);

   const auto &scriptRecipient = recvAddr.getRecipient(bs::XBTAmount{ inputAmount });
   return coinSelection.getFeeForMaxVal(scriptRecipient->getSize(), feePerByte, { input });
}

bs::core::wallet::TXSignRequest bs::tradeutils::createPayoutTXRequest(UTXO input
   , const bs::Address &recvAddr, float feePerByte, unsigned int topBlock)
{
   bs::core::wallet::TXSignRequest txReq;
   txReq.inputs.push_back(input);
   input.isInputSW_ = true;
   input.witnessDataSizeBytes_ = unsigned(bs::Address::getPayoutWitnessDataSize());
   uint64_t fee = getEstimatedFeeFor(input, recvAddr, feePerByte, topBlock);

   uint64_t value = input.getValue();
   if (value < fee) {
      value = 0;
   } else {
      value = value - fee;
   }

   txReq.fee = fee;
   txReq.recipients.emplace_back(recvAddr.getRecipient(bs::XBTAmount{ value }));
   return txReq;
}

UTXO bs::tradeutils::getInputFromTX(const bs::Address &addr
   , const BinaryData &payinHash, unsigned txOutIndex, const bs::XBTAmount& amount)
{
   constexpr uint32_t txHeight = UINT32_MAX;

   return UTXO(amount.GetValue(), txHeight, UINT32_MAX, txOutIndex, payinHash
      , BtcUtils::getP2WSHOutputScript(addr.unprefixed()));
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

               auto payinUTXO = getInputFromTX(settlAddr, args.payinTxId, 0, args.amount);

               PayoutResult result;
               result.success = true;
               result.settlementAddr = settlAddr;
               result.signRequest = createPayoutTXRequest(
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

      auto utxo = getInputFromTX(args.settlAddr, args.usedPayinHash, 0, args.amount);

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
