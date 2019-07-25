#ifndef __CC_TOKEN_ENTRY_DIALOG_H__
#define __CC_TOKEN_ENTRY_DIALOG_H__

#include <memory>
#include <QDialog>
#include "BinaryData.h"
#include "EncryptionUtils.h"

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
class SignContainer;


class CCTokenEntryDialog : public QDialog
{
Q_OBJECT

public:
   CCTokenEntryDialog(const std::shared_ptr<bs::sync::WalletsManager> &, const std::shared_ptr<CCFileManager> &
      , const std::shared_ptr<SignContainer> &, QWidget *parent);
   ~CCTokenEntryDialog() override;

protected:
   void accept() override;

private slots:
   void tokenChanged();
   void updateOkState();
   void onWalletCreated(unsigned int id, const std::shared_ptr<bs::sync::hd::Leaf> &);
   void onWalletFailed(unsigned int id, std::string errMsg);
   void onCCAddrSubmitted(const QString addr);
   void onCCInitialSubmitted(const QString addr);
   void onCCSubmitFailed(const QString addr, const QString &err);

private:
   std::unique_ptr<Ui::CCTokenEntryDialog> ui_;
   std::shared_ptr<CCFileManager>   ccFileMgr_;
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<bs::sync::Wallet>         ccWallet_;
   std::string    ccProduct_;
   std::string    strToken_;
   uint32_t       seed_ = 0;
   unsigned int   createWalletReqId_ = 0;
   bool  walletOk_ = false;
};

#endif // __CC_TOKEN_ENTRY_DIALOG_H__
