#ifndef __CREATE_WALLET_DIALOG_H__
#define __CREATE_WALLET_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BtcDefinitions.h"
#include "EncryptionUtils.h"
#include "FrejaREST.h"
#include "MetaData.h"


namespace Ui {
   class CreateWalletDialog;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
}
class SignContainer;
class WalletsManager;


class CreateWalletDialog : public QDialog
{
   Q_OBJECT

public:
   CreateWalletDialog(const std::shared_ptr<WalletsManager> &, const std::shared_ptr<SignContainer> &
      , NetworkType, const QString &walletsPath, bool createPrimary = false, QWidget *parent = nullptr);
   ~CreateWalletDialog() noexcept override = default;

   bool walletCreated() const { return walletCreated_; }
   std::string getNewWalletId() const { return walletId_; }
   bool isNewWalletPrimary() const { return createdAsPrimary_; }

private slots:
   void UpdateAcceptButtonState();
   void CreateWallet();
   void onWalletCreated(unsigned int id, std::shared_ptr<bs::hd::Wallet>);
   void onWalletCreateError(unsigned int id, std::string errMsg);
   void onPasswordChanged(const QString &);
   void onEncTypeChanged();
   void onFrejaIdChanged(const QString &);
   void startFrejaSign();
   void onFrejaSucceeded(SecureBinaryData);
   void onFrejaFailed(const QString &text);
   void onFrejaStatusUpdated(const QString &status);

protected:
   void showEvent(QShowEvent *event) override;

private:
   bool couldCreateWallet() const;

private:
   Ui::CreateWalletDialog *ui_;

private:
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   const QString     walletsPath_;
   unsigned int      createReqId_ = 0;
   bool              walletCreated_ = false;
   SecureBinaryData  walletPassword_;
   bs::wallet::Seed  walletSeed_;
   std::string       walletId_;
   FrejaSignWallet   frejaSign_;

   bool              createdAsPrimary_ = false;
};

#endif // __CREATE_WALLET_DIALOG_H__
