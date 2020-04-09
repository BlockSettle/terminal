/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CC_TOKEN_ENTRY_DIALOG_H__
#define __CC_TOKEN_ENTRY_DIALOG_H__

#include <memory>
#include <QDialog>
#include <QTimer>

#include "BinaryData.h"
#include "BSErrorCode.h"
#include "EncryptionUtils.h"
#include "ValidityFlag.h"

namespace Ui {
    class CCTokenEntryDialog;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
      namespace hd {
         class Leaf;
      }
   }
}
class CCFileManager;
class ApplicationSettings;

class CCTokenEntryDialog : public QDialog
{
Q_OBJECT

public:
   CCTokenEntryDialog(
      const std::shared_ptr<bs::sync::WalletsManager> &,
      const std::shared_ptr<CCFileManager> &,
      const std::shared_ptr<ApplicationSettings> &,
      QWidget *parent);
   ~CCTokenEntryDialog() override;

protected:
   void accept() override;
   void reject() override;

private slots:
   void tokenChanged();
   void updateOkState();
   void onCCAddrSubmitted(const QString addr);
   void onCCInitialSubmitted(const QString addr);
   void onCCSubmitFailed(const QString addr, const QString &err);

   void onTimer();
   void onCancel();
private:
   std::unique_ptr<Ui::CCTokenEntryDialog>   ui_;

   std::shared_ptr<CCFileManager>            ccFileMgr_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<bs::sync::Wallet>         ccWallet_;
   std::shared_ptr<ApplicationSettings>      settings_;

   std::string    ccProduct_;
   std::string    strToken_;
   uint32_t       seed_ = 0;

   QTimer      timer_;
   double      timeLeft_{};

   ValidityFlag validityFlag_;
};

#endif // __CC_TOKEN_ENTRY_DIALOG_H__
