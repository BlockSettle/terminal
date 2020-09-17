/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __OPEN_URI_DIALOG_H__
#define __OPEN_URI_DIALOG_H__

#include <memory>

#include <QDialog>

#include <spdlog/spdlog.h>

#include "BinaryData.h"
#include "Bip21Types.h"
#include "XBTAmount.h"

namespace Ui {
    class OpenURIDialog;
}

class QNetworkAccessManager;

class OpenURIDialog : public QDialog
{
Q_OBJECT

public:
   OpenURIDialog(const std::shared_ptr<QNetworkAccessManager>& nam
                 , bool onTestnet
                 , const std::shared_ptr<spdlog::logger> &logger
                 , QWidget *parent = nullptr);
   ~OpenURIDialog() override;

   Bip21::PaymentRequestInfo getRequestInfo() const;

private slots:
   void onURIChanged();

   void onBitpayPaymentLoaded(bool result);

signals:
   void BitpayPaymentLoaded(bool result);

private:
   bool ParseURI();

   void DisplayRequestDetails();
   void ClearRequestDetailsOnUI();

   void SetErrorText(const QString& errorText);
   void SetStatusText(const QString& statusText);
   void ClearErrorText();
   void ClearStatusText();

   void LoadPaymentOptions();

private:
   std::unique_ptr<Ui::OpenURIDialog>     ui_;
   Bip21::PaymentRequestInfo              requestInfo_;
   std::shared_ptr<QNetworkAccessManager> nam_;
   bool                                   onTestnet_;
   std::shared_ptr<spdlog::logger>        logger_;
};

#endif // __OPEN_URI_DIALOG_H__
