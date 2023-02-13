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
#include "JsonTools.h"
#include "UiUtils.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLineEdit>
#include <QNetworkReply>
#include <QPushButton>
#include <QUrl>
#include <QUrlQuery>
#include <QTimeZone>

#include <spdlog/spdlog.h>

const QString kBitcoinSchemePrefix = QStringLiteral("bitcoin:");
const QString kBitPayTestPrefix = QStringLiteral("https://test.bitpay.com/");
const QString kBitPayPrefix = QStringLiteral("https://bitpay.com/");

const QString kLabelKey = QStringLiteral("label");
const QString kMessageKey = QStringLiteral("message");
const QString kAmountKey = QStringLiteral("amount");
const QString kRequestKey = QStringLiteral("r");

OpenURIDialog::OpenURIDialog(const std::shared_ptr<QNetworkAccessManager>& nam
                 , bool onTestnet
                 , const std::shared_ptr<spdlog::logger> &logger
                 , QWidget *parent)
   : QDialog(parent)
   , ui_{new Ui::OpenURIDialog()}
   , nam_{nam}
   , onTestnet_{onTestnet}
   , logger_{logger}
{
   ui_->setupUi(this);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &QDialog::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &QDialog::reject);

   connect(ui_->lineEditURI, &QLineEdit::textEdited, this, &OpenURIDialog::onURIChanged);

   connect(this, &OpenURIDialog::BitpayPaymentLoaded, this, &OpenURIDialog::onBitpayPaymentLoaded);

   ClearErrorText();
   ClearStatusText();
   ClearRequestDetailsOnUI();
}

OpenURIDialog::~OpenURIDialog() = default;

void OpenURIDialog::onURIChanged()
{
   ClearErrorText();
   ClearStatusText();

   if (ParseURI()) {
      DisplayRequestDetails();

      if (!requestInfo_.requestURL.isEmpty()) {
         //
         LoadPaymentOptions();
      } else {
         ui_->pushButtonOk->setEnabled(true);
      }
   } else {
      ClearRequestDetailsOnUI();
      ui_->pushButtonOk->setEnabled(false);
   }
}

void OpenURIDialog::LoadPaymentOptions()
{
   SetStatusText(tr("Loading BitPay request details."));

   ui_->pushButtonOk->setEnabled(false);
   ui_->lineEditURI->setEnabled(false);

   // send request
#if 0 //BitPay is not supported now
   QNetworkRequest request = BitPay::getBTCPaymentRequest(requestInfo_.requestURL);
   QNetworkReply *reply = nam_->post(request, BitPay::getBTCPaymentRequestPayload());

   connect(reply, &QNetworkReply::finished, this, [this, reply] {
      if (reply->error() == QNetworkReply::NoError) {
         do {
            const auto replyData = reply->readAll();

            logger_->debug("[OpenURIDialog::LoadPaymentOptions cb] response\n{}"
               , replyData.toStdString());

            decltype(Bip21::PaymentRequestInfo::requestExpireDateTime)  requestExpireDateTime;
            decltype(Bip21::PaymentRequestInfo::requestMemo)            requestMemo;
            decltype(Bip21::PaymentRequestInfo::feePerByte)             feePerByte;
            decltype(Bip21::PaymentRequestInfo::amount)                 amount;
            decltype(Bip21::PaymentRequestInfo::address)                address;

            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(replyData, &error);
            if (error.error != QJsonParseError::NoError) {
               SetErrorText(tr("Invalid response from BitPay"));
               const auto errorMessage = error.errorString().toStdString();
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] failed to parse response: {}"
                  , errorMessage);
               break;
            }

            const QVariantMap response = doc.object().toVariantMap();

            std::unordered_map<std::string, std::string> responseHeaders;
            for (const auto& i : reply->rawHeaderPairs()) {
               responseHeaders.emplace(i.first.toStdString(), i.second.toStdString());
            }

            // check required headers
            // XXX

            // digest - A SHA256 hash of the JSON response string, should be
            //          verified by the client before proceeding

            // x-identity - An identifier to represent which public key should
            //                be used to verify the signature. For example for
            //                BitPay's ECC keys we will include the public key
            //                hash in this header. Implementations should NOT
            //                include the public key here directly.

            // x-signature-type - The signature format used to sign the payload.
            //                   For the foreseeable future BitPay will always
            //                   use ECC. However, we wanted to grant some
            //                   flexibility to the specification.

            // x-signature - A cryptographic signature of the SHA256 hash of the
            //                payload. This is to prove that the payment request
            //                was not tampered with before being received by the wallet.

            // https://github.com/bitpay/jsonPaymentProtocol/blob/master/v2/specification.md

            // check response field
            const auto expireDateString = JsonTools::GetStringProperty(response, QString::fromStdString("expires"));
            if (expireDateString.isEmpty()) {
               SetErrorText(tr("Invalid response from BitPay"));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] missing \"expires\" in response");
               break;
            }

            requestExpireDateTime = QDateTime::fromString(expireDateString, Qt::ISODate);
            if (!requestExpireDateTime.isValid()) {
               SetErrorText(tr("Invalid response from BitPay"));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] failed to parse date: {}"
                              , expireDateString.toStdString());
               break;
            }


            requestExpireDateTime = requestExpireDateTime.toLocalTime();

            requestMemo = JsonTools::GetStringProperty(response, QString::fromStdString("memo"));
            if (requestMemo.isEmpty()) {
               SetErrorText(tr("Invalid response from BitPay"));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] missing \"memo\" in response");
               break;
            }

            const auto networkString = JsonTools::GetStringProperty(response, QString::fromStdString("network"));
            if (networkString.isEmpty()) {
               SetErrorText(tr("Invalid response from BitPay"));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] missing \"network\" in response");
               break;
            }

            const QString expectedNetwork = onTestnet_ ? QStringLiteral("test") : QStringLiteral("main");
            if (networkString.toLower() != expectedNetwork) {
               SetErrorText(tr("Network mismatch in request"));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] {} in request. {} expected"
                              , networkString.toStdString(), expectedNetwork.toStdString());
               break;
            }

            auto instructionsArray = response[QStringLiteral("instructions")].toList();
            if (instructionsArray.isEmpty()) {
               SetErrorText(tr("Invalid response from BitPay"));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] empty instructions list");
               break;
            }

            auto instructions = instructionsArray[0].toMap();
            {
               bool converted = false;
               feePerByte = JsonTools::GetDoubleProperty(instructions, QStringLiteral("requiredFeeRate"), &converted);

               if (!converted) {
                  SetErrorText(tr("Invalid response from BitPay"));
                  logger_->error("[OpenURIDialog::LoadPaymentOptions cb] fee undefined in response");
                  break;
               }
            }

            auto outputs = instructions[QStringLiteral("outputs")].toList();
            if (outputs.isEmpty()) {
               SetErrorText(tr("Invalid response from BitPay"));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] empty outputs list in instruction");
               break;
            }

            {
               auto paymentInfo = outputs[0].toMap();

               bool converted = false;
               uint64_t amountValue = JsonTools::GetUIntProperty(paymentInfo, QStringLiteral("amount"), &converted);
               if (!converted || amountValue == 0 ) {
                  SetErrorText(tr("Invalid response from BitPay"));
                  logger_->error("[OpenURIDialog::LoadPaymentOptions cb] invalid amount in response");
                  break;
               }
               amount.SetValue(amountValue);

               address = JsonTools::GetStringProperty(paymentInfo, QStringLiteral("address"));
               bool addressValid = false;
               try {
                  if (!address.isEmpty()) {
                     bs::Address addressObj = bs::Address::fromAddressString(address.toStdString());
                     addressValid = addressObj.isValid();
                  }
               } catch(...) {
               }

               if (!addressValid) {
                  SetErrorText(tr("Invalid response from BitPay"));
                  logger_->error("[OpenURIDialog::LoadPaymentOptions cb] invalid address in response {}"
                                 , address.toStdString());
                  break;
               }
            }

            //validate data if it was set before as part of Bip21 string
            if (!requestInfo_.address.isEmpty()) {
               if (requestInfo_.address != address) {
                  SetErrorText(tr("Receiving address mismatch."));
                  logger_->error("[OpenURIDialog::LoadPaymentOptions cb] address mismatch: {} in bip string, {} returned from request"
                                 , requestInfo_.address.toStdString(), address.toStdString());
                  break;
               }
            }

            if (requestInfo_.amount.GetValue() != 0 && requestInfo_.amount != amount) {
               SetErrorText(tr("Receiving amount mismatch."));
               logger_->error("[OpenURIDialog::LoadPaymentOptions cb] amount mismatch: {} in bip string, {} returned from request"
                              , requestInfo_.amount.GetValue(), amount.GetValue());
               break;
            }

            requestInfo_.requestExpireDateTime = requestExpireDateTime;
            requestInfo_.requestMemo = requestMemo;
            requestInfo_.feePerByte = feePerByte;
            requestInfo_.amount = amount;
            requestInfo_.address = address;

            emit BitpayPaymentLoaded(true);
            return;
         } while (false);
      } else {
         SetErrorText(tr("Failed to get response from BitPay"));
         logger_->error("[OpenURIDialog::LoadPaymentOptions cb] request to {} failed {}"
                        , requestInfo_.requestURL.toStdString()
                        , reply->errorString().toStdString());
      }

      emit BitpayPaymentLoaded(false);
   });

   connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
   connect(this, &OpenURIDialog::finished, reply, &QNetworkReply::abort);
#endif   //0
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

   Bip21::PaymentRequestInfo info;

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
   const auto items = uriQuery.queryItems(QUrl::FullyDecoded);
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

         QUrl requestUri{requestURL};
         if (!requestUri.isValid()) {
            SetErrorText(tr("Invalid request URL."));
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

      ui_->groupBoxBitPayDetails->setVisible(true);

      if (requestInfo_.requestExpireDateTime.isValid()) {
         ui_->labelDetailsExpires->setText(UiUtils::displayDateTime(requestInfo_.requestExpireDateTime));
      } else {
         ui_->labelDetailsExpires->clear();
      }

      ui_->labelDetailsFeeRate->setText(tr("%1 s/b").arg(requestInfo_.feePerByte));
      ui_->labelDetailsMemo->setText(requestInfo_.requestMemo);
   }
}

void OpenURIDialog::ClearRequestDetailsOnUI()
{
   requestInfo_ = Bip21::PaymentRequestInfo{};

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

Bip21::PaymentRequestInfo OpenURIDialog::getRequestInfo() const
{
   return requestInfo_;
}

void OpenURIDialog::onBitpayPaymentLoaded(bool result)
{
   ClearStatusText();

   ui_->lineEditURI->setEnabled(true);

   if (result) {
      DisplayRequestDetails();
      ui_->pushButtonOk->setEnabled(true);
   } else {
      ui_->groupBoxBitPayDetails->setVisible(false);
   }
}
