#ifndef __CC_TOKEN_ENTRY_DIALOG_H__
#define __CC_TOKEN_ENTRY_DIALOG_H__

#include <memory>
#include <QDialog>
#include "BinaryData.h"


namespace Ui {
    class CCTokenEntryDialog;
}
namespace bs {
   class Wallet;
}
class CCFileManager;
class SignContainer;
class WalletsManager;


class CCTokenEntryDialog : public QDialog
{
Q_OBJECT

public:
   CCTokenEntryDialog(const std::shared_ptr<WalletsManager> &, const std::shared_ptr<CCFileManager> &
      , const std::shared_ptr<SignContainer> &, QWidget *parent);
   ~CCTokenEntryDialog() override = default;

private slots:
   void accept() override;
   void tokenChanged();
   void passwordChanged();
   void updateOkState();
   void onWalletCreated(unsigned int id, BinaryData pubKey, BinaryData chainCode, std::string walletId);
   void onWalletFailed(unsigned int id, std::string errMsg);
   void onCCAddrSubmitted(const QString addr);

private:
   Ui::CCTokenEntryDialog* ui_;
   std::shared_ptr<CCFileManager>   ccFileMgr_;
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<WalletsManager>  walletsMgr_;
   std::string    ccProduct_;
   uint32_t       seed_ = 0;
   unsigned int   createWalletReqId_ = 0;
   std::shared_ptr<bs::Wallet>      ccWallet_;
   bool  walletOk_ = false;
   bool  passwordOk_ = false;
};

#endif // __CC_TOKEN_ENTRY_DIALOG_H__
