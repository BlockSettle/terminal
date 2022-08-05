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
#include "Wallets/HeadlessContainer.h"
#include "PasswordDialogDataWrapper.h"
#include "Wallets/TradesUtils.h"
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

   actionRBF_ = new QAction(tr("Replace-By-Fee (RBF)"), this);
   connect(actionRBF_, &QAction::triggered, this, &TransactionsWidgetInterface::onCreateRBFDialog);

   actionCPFP_ = new QAction(tr("Child-Pays-For-Parent (CPFP)"), this);
   connect(actionCPFP_, &QAction::triggered, this, &TransactionsWidgetInterface::onCreateCPFPDialog);
}

void TransactionsWidgetInterface::init(const std::shared_ptr<spdlog::logger> &logger)
{
   logger_ = logger;
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