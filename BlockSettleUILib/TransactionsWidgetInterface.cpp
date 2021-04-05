/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "TransactionsWidgetInterface.h"
#include "ApplicationSettings.h"
#include "BSMessageBox.h"
#include "CreateTransactionDialogAdvanced.h"
#include "HeadlessContainer.h"
#include "PasswordDialogDataWrapper.h"
#include "TradesUtils.h"
#include "TransactionsViewModel.h"
#include "TransactionDetailDialog.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "UiUtils.h"
#include "UtxoReservationManager.h"
#include <spdlog/spdlog.h>
#include <QApplication>
#include <QClipboard>


TransactionsWidgetInterface::TransactionsWidgetInterface(QWidget *parent)
   : TabWithShortcut(parent)
{
   actionCopyAddr_ = new QAction(tr("&Copy Address"));
   connect(actionCopyAddr_, &QAction::triggered, this, [this]() {
      qApp->clipboard()->setText(curAddress_);
   });

   actionCopyTx_ = new QAction(tr("Copy &Transaction Hash"));
   connect(actionCopyTx_, &QAction::triggered, this, [this]() {
      qApp->clipboard()->setText(curTx_);
   });

   actionRevoke_ = new QAction(tr("Revoke"), this);
   connect(actionRevoke_, &QAction::triggered, this, &TransactionsWidgetInterface::onRevokeSettlement);

   actionRBF_ = new QAction(tr("Replace-By-Fee (RBF)"), this);
   connect(actionRBF_, &QAction::triggered, this, &TransactionsWidgetInterface::onCreateRBFDialog);

   actionCPFP_ = new QAction(tr("Child-Pays-For-Parent (CPFP)"), this);
   connect(actionCPFP_, &QAction::triggered, this, &TransactionsWidgetInterface::onCreateCPFPDialog);
}

void TransactionsWidgetInterface::init(const std::shared_ptr<spdlog::logger> &logger)
{
   logger_ = logger;
}

void TransactionsWidgetInterface::onRevokeSettlement()
{
   auto txItem = model_->getItem(actionRevoke_->data().toModelIndex());
   if (!txItem) {
      SPDLOG_LOGGER_ERROR(logger_, "item not found");
      return;
   }
   auto args = std::make_shared<bs::tradeutils::PayoutArgs>();

   auto payoutCb = bs::tradeutils::PayoutResultCb([this, args, txItem]
   (bs::tradeutils::PayoutResult result)
   {
      const auto &timestamp = QDateTime::currentDateTimeUtc();
      QMetaObject::invokeMethod(qApp, [this, args, txItem, timestamp, result] {
         if (!result.success) {
            SPDLOG_LOGGER_ERROR(logger_, "creating payout failed: {}", result.errorMsg);
            BSMessageBox(BSMessageBox::critical, tr("Revoke Transaction")
               , tr("Revoke failed")
               , tr("failed to create pay-out TX"), this).exec();
            return;
         }

         constexpr int kRevokeTimeout = 60;
         const auto &settlementIdHex = args->settlementId.toHexStr();
         bs::sync::PasswordDialogData dlgData;
         dlgData.setValue(bs::sync::PasswordDialogData::SettlementId, QString::fromStdString(settlementIdHex));
         dlgData.setValue(bs::sync::PasswordDialogData::Title, tr("Settlement Revoke"));
         dlgData.setValue(bs::sync::PasswordDialogData::DurationLeft, kRevokeTimeout * 1000);
         dlgData.setValue(bs::sync::PasswordDialogData::DurationTotal, kRevokeTimeout * 1000);
         dlgData.setValue(bs::sync::PasswordDialogData::SettlementPayOutVisible, true);

         // Set timestamp that will be used by auth eid server to update timers.
         dlgData.setValue(bs::sync::PasswordDialogData::DurationTimestamp, static_cast<int>(timestamp.toSecsSinceEpoch()));

         dlgData.setValue(bs::sync::PasswordDialogData::ProductGroup, tr(bs::network::Asset::toString(bs::network::Asset::SpotXBT)));
         dlgData.setValue(bs::sync::PasswordDialogData::Security, txItem->comment);
         dlgData.setValue(bs::sync::PasswordDialogData::Product, "XXX");
         dlgData.setValue(bs::sync::PasswordDialogData::Side, tr("Revoke"));
         dlgData.setValue(bs::sync::PasswordDialogData::Price, tr("N/A"));

         dlgData.setValue(bs::sync::PasswordDialogData::Market, "XBT");
         dlgData.setValue(bs::sync::PasswordDialogData::SettlementId, settlementIdHex);
         dlgData.setValue(bs::sync::PasswordDialogData::RequesterAuthAddressVerified, true);
         dlgData.setValue(bs::sync::PasswordDialogData::ResponderAuthAddressVerified, true);
         dlgData.setValue(bs::sync::PasswordDialogData::SigningAllowed, true);
                          
         dlgData.setValue(bs::sync::PasswordDialogData::ExpandTxInfo,
            appSettings_->get(ApplicationSettings::AdvancedTxDialogByDefault).toBool());

         const auto amount = args->amount.GetValueBitcoin();
         SPDLOG_LOGGER_DEBUG(logger_, "revoke fee={}, qty={} ({}), recv addr: {}"
            ", settl addr: {}", result.signRequest.fee, amount
            , amount * BTCNumericTypes::BalanceDivider, args->recvAddr.display()
            , result.settlementAddr.display());

         //note: signRequest should be a shared_ptr
         auto signObj = result.signRequest;
         const auto reqId = signContainer_->signSettlementPayoutTXRequest(signObj
            , { args->settlementId, args->cpAuthPubKey, false }, dlgData);
         if (reqId) {
            revokeIds_.insert(reqId);
         }
         else {
            BSMessageBox(BSMessageBox::critical, tr("Revoke Transaction")
               , tr("Revoke failed")
               , tr("failed to send TX request to signer"), this).exec();
         }
      });
   });

   const auto &cbSettlAuth = [this, args, payoutCb](const bs::Address &ownAuthAddr)
   {
      if (ownAuthAddr.empty()) {
         QMetaObject::invokeMethod(this, [this] {
            BSMessageBox(BSMessageBox::critical, tr("Revoke Transaction")
               , tr("Failed to create revoke transaction")
               , tr("auth wallet doesn't contain settlement metadata"), this).exec();
         });
         return;
      }
      args->ourAuthAddress = ownAuthAddr;
      bs::tradeutils::createPayout(*args, payoutCb, false);
   };
   const auto &cbSettlCP = [this, args, cbSettlAuth]
   (const BinaryData &settlementId, const BinaryData &dealerAuthKey)
   {
      if (settlementId.empty() || dealerAuthKey.empty()) {
         cbSettlAuth({});
         return;
      }
      args->settlementId = settlementId;
      args->cpAuthPubKey = dealerAuthKey;
      signContainer_->getSettlAuthAddr(walletsManager_->getPrimaryWallet()->walletId()
         , settlementId, cbSettlAuth);
   };
   const auto &cbDialog = [this, args, cbSettlCP]
   (const TransactionPtr &txItem)
   {
      for (int i = 0; i < txItem->tx.getNumTxOut(); ++i) {
         const auto &txOut = txItem->tx.getTxOutCopy(i);
         const auto &addr = bs::Address::fromTxOut(txOut);
         if (addr.getType() == AddressEntryType_P2WSH) {
            args->amount = bs::XBTAmount{ txOut.getValue() };
            break;
         }
      }

      const auto &xbtWallet = walletsManager_->getDefaultWallet();
      args->walletsMgr = walletsManager_;
      args->armory = armory_;
      args->signContainer = signContainer_;
      args->payinTxId = txItem->txEntry.txHash;
      args->outputXbtWallet = xbtWallet;

      xbtWallet->getNewExtAddress([this, args, cbSettlCP](const bs::Address &addr) {
         args->recvAddr = addr;
         signContainer_->getSettlCP(walletsManager_->getPrimaryWallet()->walletId()
            , args->payinTxId, cbSettlCP);
      });
   };

   if (txItem->initialized) {
      cbDialog(txItem);
   }
}

void TransactionsWidgetInterface::onCreateRBFDialog()
{
   const auto &txItem = model_->getItem(actionRBF_->data().toModelIndex());
   if (!txItem) {
      SPDLOG_LOGGER_ERROR(logger_, "item not found");
      return;
   }

   const auto &cbDialog = [this](const TransactionPtr &txItem) {
      try {
         //FIXME: auto dlg = CreateTransactionDialogAdvanced::CreateForRBF(topBlock_
         //   , logger_, txItem->tx, this);
         //dlg->exec();
      }
      catch (const std::exception &e) {
         BSMessageBox(BSMessageBox::critical, tr("RBF Transaction"), tr("Failed to create RBF transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem->initialized) {
      cbDialog(txItem);
   }
}

void TransactionsWidgetInterface::onCreateCPFPDialog()
{
   const auto &txItem = model_->getItem(actionCPFP_->data().toModelIndex());
   if (!txItem) {
      SPDLOG_LOGGER_ERROR(logger_, "item not found");
      return;
   }

   const auto &cbDialog = [this](const TransactionPtr &txItem) {
      try {
         std::shared_ptr<bs::sync::Wallet> wallet;
         for (const auto &w : txItem->wallets) {
            if (w->type() == bs::core::wallet::Type::Bitcoin) {
               wallet = w;
               break;
            }
         }
         //FIXME: auto dlg = CreateTransactionDialogAdvanced::CreateForCPFP(topBlock_
         //   , logger_, , txItem->tx, this);
         //dlg->exec();
      }
      catch (const std::exception &e) {
         BSMessageBox(BSMessageBox::critical, tr("CPFP Transaction"), tr("Failed to create CPFP transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem->initialized) {
      cbDialog(txItem);
   }
}

void TransactionsWidgetInterface::onTXSigned(unsigned int id, BinaryData signedTX
   , bs::error::ErrorCode errCode, std::string errTxt)
{
   if (revokeIds_.find(id) != revokeIds_.end()) {
      revokeIds_.erase(id);
      if (errCode == bs::error::ErrorCode::TxCancelled) {
         SPDLOG_LOGGER_INFO(logger_, "revoke {} cancelled", id);
         return;
      }

      if ((errCode != bs::error::ErrorCode::NoError) || signedTX.empty()) {
         logger_->warn("[TransactionsWidget::onTXSigned] revoke sign failure: {} ({})"
            , (int)errCode, errTxt);
         QMetaObject::invokeMethod(this, [this, errTxt] {
            BSMessageBox(BSMessageBox::critical, tr("Revoke Transaction")
               , tr("Failed to sign revoke transaction")
               , QString::fromStdString(errTxt), this).exec();
         });
         return;
      }
      SPDLOG_LOGGER_DEBUG(logger_, "signed revoke: {}", signedTX.toHexStr());

      if (armory_->pushZC(signedTX).empty()) {
         BSMessageBox(BSMessageBox::critical, tr("Revoke Transaction")
            , tr("Failed to send revoke transaction")
            , tr("BlockSettleDB connection unavailable"), this).exec();
      }
   }
}