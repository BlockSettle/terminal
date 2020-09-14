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

#include "BinaryData.h"
#include "XBTAmount.h"

namespace Ui {
    class OpenURIDialog;
}

class OpenURIDialog : public QDialog
{
Q_OBJECT

public:
   struct PaymentRequestInfo
   {
      QString        address;
      bs::XBTAmount  amount;
      QString        label;
      QString        message;
      QString        requestURL;
   };

public:
   OpenURIDialog(QWidget *parent);
   ~OpenURIDialog() override;

   PaymentRequestInfo getRequestInfo() const;

private slots:
   void onURIChanhed();

private:
   bool ParseURI();

   void DisplayRequestDetails();
   void ClearRequestDetailsOnUI();

   void SetErrorText(const QString& errorText);
   void SetStatusText(const QString& statusText);
   void ClearErrorText();
   void ClearStatusText();

private:
   std::unique_ptr<Ui::OpenURIDialog>  ui_;
   PaymentRequestInfo                  requestInfo_;
};

#endif // __OPEN_URI_DIALOG_H__
