#ifndef __AUTH_ADDRESS_DIALOG_H__
#define __AUTH_ADDRESS_DIALOG_H__

#include "AuthAddressManager.h"
#include "BinaryData.h"

#include <memory>

#include <QDialog>

class AssetManager;
class AuthAddressViewModel;
class QItemSelection;
class OTPManager;

namespace Ui {
    class AuthAddressDialog;
};

class AuthAddressDialog : public QDialog
{
Q_OBJECT

public:
   AuthAddressDialog(const std::shared_ptr<AuthAddressManager>& authAddressManager
      , const std::shared_ptr<AssetManager> &, const std::shared_ptr<OTPManager> &
      , const std::shared_ptr<ApplicationSettings> &, QWidget* parent = nullptr);
   ~AuthAddressDialog() override = default;

   void setAddressToVerify(const QString &addr);

signals:
   void askForConfirmation(const QString &address, double txAmount);

private slots:
   void resizeTreeViewColumns();
   void adressSelected(const QItemSelection &selected, const QItemSelection &deselected);

   void createAddress();
   void revokeSelectedAddress();
   void submitSelectedAddress();
   void verifySelectedAddress();
   void setDefaultAddress();

   void onModelReset();
   void onAddressListUpdated();
   void onAddressStateChanged(const QString &addr, const QString &state);

   void onAuthMgrError(const QString &details);
   void onAuthMgrInfo(const QString &text);

   void onAuthAddressConfirmationRequired(float validationAmount);
   void onAuthAddrSubmitError(const QString &address, const QString &error);
   void onAuthAddrSubmitSuccess(const QString &address);

   void ConfirmAuthAddressSubmission();

   void onOtpSignFailed();

   void onAuthVerifyTxSent();

protected:
   void showEvent(QShowEvent *) override;
   void showError(const QString &text, const QString &details = {});
   void showInfo(const QString &title, const QString &text);

private:
   bs::Address GetSelectedAddress() const;
   bool unsubmittedExist() const;
   void updateUnsubmittedState();

private:
   Ui::AuthAddressDialog*  ui_;
   std::shared_ptr<AuthAddressManager>    authAddressManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<OTPManager>            otpManager_;
   std::shared_ptr<ApplicationSettings>   settings_;
   AuthAddressViewModel                *  model_;
   bs::Address                            defaultAddr_;

   bs::Address                            lastSubmittedAddress_;

   bool  unconfirmedExists_ = false;
};

#endif // __AUTH_ADDRESS_DIALOG_H__
