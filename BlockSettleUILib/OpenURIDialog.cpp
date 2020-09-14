/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "OpenURIDialog.h"

#include "ui_OpenURIDialog.h"

#include "Address.h"
#include "UiUtils.h"

#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QUrlQuery>

#include <spdlog/spdlog.h>

const QString kBitcoinSchemePrefix = QStringLiteral("bitcoin:");
const QString kBitPayTestPrefix = QStringLiteral("https://test.bitpay.com/");
const QString kBitPayPrefix = QStringLiteral("https://bitpay.com/");

const QString kLabelKey = QStringLiteral("label");
const QString kMessageKey = QStringLiteral("message");
const QString kAmountKey = QStringLiteral("amount");
const QString kRequestKey = QStringLiteral("r");

OpenURIDialog::OpenURIDialog(QWidget *parent)
   : QDialog(parent)
   , ui_(new Ui::OpenURIDialog())
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &QDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &QDialog::reject);

   connect(ui_->lineEditURI, &QLineEdit::textEdited, this, &OpenURIDialog::onURIChanhed);

   ClearErrorText();
   ClearStatusText();
   ClearRequestDetailsOnUI();
}

OpenURIDialog::~OpenURIDialog() = default;

void OpenURIDialog::onURIChanhed()
{
   ClearErrorText();
   ClearStatusText();

   if (ParseURI()) {
      DisplayRequestDetails();
      ui_->pushButtonOk->setEnabled(true);
   } else {
      ClearRequestDetailsOnUI();
      ui_->pushButtonOk->setEnabled(false);
   }
}

bool OpenURIDialog::ParseURI()
{
   const auto text = ui_->lineEditURI->text().trimmed();
   if (text.isEmpty() || text.length() < kBitcoinSchemePrefix.length()) {
      return false;
   }

   if (!text.startsWith(kBitcoinSchemePrefix, Qt::CaseInsensitive)) {
      SetErrorText(tr("Invalid schema"));
      return false;
   }

   QUrl uri{text};

   if (!uri.isValid()) {
      return false;
   }

   PaymentRequestInfo info;

   info.address = uri.path();

   if (!info.address.isEmpty()) {

      bs::Address address;
      try {
         address = bs::Address::fromAddressString(info.address.toStdString());
      } catch(...) {
      }

      if (!address.isValid()) {
         SetErrorText(tr("Invalid address format. Check Url or network"));
         return false;
      }
   }

   QUrlQuery uriQuery(uri);
   const auto items = uriQuery.queryItems();
   for (const auto& i : items) {
      if (i.first == kLabelKey) {
         info.label = i.second;
      } else if (i.first == kMessageKey) {
         info.message = i.second;
      } else if (i.first == kAmountKey) {
         bool converted = false;
         double value = UiUtils::parseAmountBtc(i.second, &converted);

         if (!converted || value < 0) {
            SetErrorText(tr("Invalid amount format."));
            return false;
         }

         info.amount.SetValueBitcoin(value);
         if (info.amount.GetValue() == 0) {
            SetErrorText(tr("Amount could not be 0."));
            return false;
         }
      } else if (i.first == kRequestKey) {
         // check that request is from bit pay
         // reject if not
         // BIP70 is not supported by default in bitcoin-qt client

         const auto requestURL = i.second;
         if (!(requestURL.startsWith(kBitPayTestPrefix, Qt::CaseInsensitive) || requestURL.startsWith(kBitPayPrefix, Qt::CaseInsensitive))) {
            SetErrorText(tr("BIP70 supported for BitPay only."));
            return false;
         }

         info.requestURL = requestURL;
      }
   }

   requestInfo_ = info;

   return true;
}

void OpenURIDialog::DisplayRequestDetails()
{
   ui_->labelDetailsAddress->setText(requestInfo_.address);
   ui_->labelDetailsLabel->setText(requestInfo_.label);
   ui_->labelDetailsMessage->setText(requestInfo_.message);

   if (requestInfo_.amount.GetValue() == 0) {
      ui_->labelDetilsAmount->clear();
   } else {
      ui_->labelDetilsAmount->setText(UiUtils::displayAmount(requestInfo_.amount));
   }

   if (!requestInfo_.requestURL.isEmpty()) {
      ui_->labelRequestURL->setText(tr("<a href=\"%1\"><span style=\"font-size:12px; text-decoration: underline; color:#fefeff\">%1</span></a>").arg(requestInfo_.requestURL));
   }
}

void OpenURIDialog::ClearRequestDetailsOnUI()
{
   requestInfo_ = PaymentRequestInfo{};

   ui_->labelDetailsAddress->clear();
   ui_->labelDetailsLabel->clear();
   ui_->labelDetailsMessage->clear();
   ui_->labelDetilsAmount->clear();
   ui_->labelRequestURL->clear();

   ui_->groupBoxBitPayDetails->hide();
   ui_->labelDetailsExpires->clear();
   ui_->labelDetailsFeeRate->clear();
}

void OpenURIDialog::SetErrorText(const QString& errorText)
{
   ui_->labelErrorMessage->setText(errorText);
   ui_->labelErrorMessage->setVisible(true);
   ui_->labelStatus->setVisible(false);
}

void OpenURIDialog::SetStatusText(const QString& statusText)
{
   ui_->labelStatus->setText(statusText);
   ui_->labelErrorMessage->setVisible(false);
   ui_->labelStatus->setVisible(true);
}

void OpenURIDialog::ClearErrorText()
{
   ui_->labelErrorMessage->setVisible(false);
}

void OpenURIDialog::ClearStatusText()
{
   ui_->labelStatus->setVisible(false);
}

OpenURIDialog::PaymentRequestInfo OpenURIDialog::getRequestInfo() const
{
   return requestInfo_;
}
