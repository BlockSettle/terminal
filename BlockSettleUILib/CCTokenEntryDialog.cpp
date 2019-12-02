/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CCTokenEntryDialog.h"
#include "ui_CCTokenEntryDialog.h"

#include "bs_communication.pb.h"

#include <QLineEdit>
#include <QPushButton>
#include <spdlog/spdlog.h>

#include "BSErrorCodeStrings.h"
#include "BSMessageBox.h"
#include "CCFileManager.h"
#include "SignContainer.h"
#include "Wallets/SyncHDLeaf.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "BSErrorCodeStrings.h"

namespace {
   const auto kAutheIdTimeout = int(BsClient::autheidCcAddressTimeout() / std::chrono::seconds(1));
}

CCTokenEntryDialog::CCTokenEntryDialog(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<CCFileManager> &ccFileMgr
      , QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::CCTokenEntryDialog())
   , ccFileMgr_(ccFileMgr)
   , walletsMgr_(walletsMgr)
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &CCTokenEntryDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &CCTokenEntryDialog::onCancel);
   connect(ui_->lineEditToken, &QLineEdit::textEdited, this, &CCTokenEntryDialog::tokenChanged);

   connect(ccFileMgr_.get(), &CCFileManager::CCAddressSubmitted, this, &CCTokenEntryDialog::onCCAddrSubmitted, Qt::QueuedConnection);
   connect(ccFileMgr_.get(), &CCFileManager::CCInitialSubmitted, this, &CCTokenEntryDialog::onCCInitialSubmitted, Qt::QueuedConnection);
   connect(ccFileMgr_.get(), &CCFileManager::CCSubmitFailed, this, &CCTokenEntryDialog::onCCSubmitFailed, Qt::QueuedConnection);

   ccFileMgr_->LoadCCDefinitionsFromPub();

   updateOkState();

   ui_->progressBar->setMaximum(kAutheIdTimeout * 10);

   timer_.setInterval(100);
   connect(&timer_, &QTimer::timeout, this, &CCTokenEntryDialog::onTimer);
}

CCTokenEntryDialog::~CCTokenEntryDialog() = default;

void CCTokenEntryDialog::tokenChanged()
{
   ui_->labelTokenHint->clear();
   ui_->pushButtonOk->setEnabled(false);
   strToken_ = ui_->lineEditToken->text().toStdString();
   if (strToken_.empty()) {
      return;
   }

   try {
      BinaryData base58In(strToken_);
      base58In.append('\0'); // Remove once base58toScrAddr() is fixed.
      const auto decoded = BtcUtils::base58toScrAddr(base58In).toBinStr();
      Blocksettle::Communication::CCSeedResponse response;
      if (!response.ParseFromString(decoded)) {
         throw std::invalid_argument("invalid internal token structure");
      }
      seed_ = response.bsseed();
      ccProduct_ = response.ccproduct();

      ccWallet_ = walletsMgr_->getCCWallet(ccProduct_);
      if (!ccWallet_) {
         ui_->labelTokenHint->setText(tr("The Terminal will prompt you to create the relevant subwallet (%1),"
            " if required").arg(QString::fromStdString(ccProduct_)));

         MessageBoxCCWalletQuestion qry(QString::fromStdString(ccProduct_), this);
         if (qry.exec() == QDialog::Accepted) {
            const auto createCCLeafCb = [this](bs::error::ErrorCode result){
               if (result == bs::error::ErrorCode::TxCanceled) {
                  reject();
               }
               else if (result == bs::error::ErrorCode::NoError) {
                  ccWallet_ = walletsMgr_->getCCWallet(ccProduct_);
                  if (ccWallet_) {
                     ui_->labelTokenHint->setText(tr("Private Market subwallet for %1 created!").arg(QString::fromStdString(ccProduct_)));
                  } else {
                     ui_->labelTokenHint->setText(tr("Failed to create CC subwallet %1").arg(QString::fromStdString(ccProduct_)));
                  }
                  updateOkState();
               }
               else {
                  ui_->labelTokenHint->setText(tr("Failed to create CC subwallet %1, reason:\n%2")
                                               .arg(QString::fromStdString(ccProduct_))
                                               .arg(bs::error::ErrorCodeToString(result)));
               }
            };

            walletsMgr_->CreateCCLeaf(ccProduct_, createCCLeafCb);

         } else {
            reject();
         }
      }
   }
   catch (const std::exception &e) {
      ui_->labelTokenHint->setText(tr("Token you entered is not valid: %1").arg(QLatin1String(e.what())));
   }
   updateOkState();
}

void CCTokenEntryDialog::updateOkState()
{
   ui_->pushButtonOk->setEnabled(ccWallet_ != nullptr);
}

void CCTokenEntryDialog::accept()
{
   if (!ccWallet_) {
      reject();
      return;
   }
   const auto &cbAddr = [this](const bs::Address &address) {
      if (ccFileMgr_->SubmitAddressToPuB(address, seed_, strToken_)) {
         ui_->pushButtonOk->setEnabled(false);
      } else {
         onCCSubmitFailed(QString::fromStdString(address.display())
            , tr("Submission to PB failed"));
      }
   };
   ccWallet_->getNewExtAddress(cbAddr);

   ui_->progressBar->setValue(kAutheIdTimeout * 10);
   timeLeft_ = kAutheIdTimeout;
   timer_.start();
   ui_->stackedWidgetAuth->setCurrentWidget(ui_->pageAuth);
   ui_->labelToken->setText(ui_->lineEditToken->text());
}

void CCTokenEntryDialog::onCCAddrSubmitted(const QString)
{
   QDialog::accept();
   BSMessageBox(BSMessageBox::success, tr("Submission Successful")
      , tr("Equity token submitted")
      , tr("Please allow up to 24h for the transaction to be broadcast.")).exec();
}

void CCTokenEntryDialog::onCCInitialSubmitted(const QString)
{
   ui_->labelTokenHint->setText(tr("Request was sent"));
}

void CCTokenEntryDialog::onCCSubmitFailed(const QString, const QString &err)
{
   reject();
   BSMessageBox(BSMessageBox::critical, tr("CC Token submit failure")
      , tr("Failed to submit Private Market token to BlockSettle"), err, this).exec();
}

void CCTokenEntryDialog::onTimer()
{
   timeLeft_ -= timer_.interval() / 1000.0;

   if (timeLeft_ < 0) {
      return;
   }

   ui_->progressBar->setValue(int(timeLeft_ * 10));
   ui_->labelTimeLeft->setText(tr("%1 seconds left").arg(int(timeLeft_)));
}

void CCTokenEntryDialog::onCancel()
{
   ui_->stackedWidgetAuth->setCurrentWidget(ui_->pageEnter);
   timeLeft_ = 0;
   timer_.stop();

   // FIXME:
   // implement cancelation
}
