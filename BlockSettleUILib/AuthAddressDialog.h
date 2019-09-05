#ifndef __AUTH_ADDRESS_DIALOG_H__
#define __AUTH_ADDRESS_DIALOG_H__

#include "AuthAddressManager.h"
#include "BinaryData.h"
#include "BsClient.h"
#include <memory>
#include <QDialog>
#include <QPointer>

class AssetManager;
class AuthAddressViewModel;
class QItemSelection;

namespace Ui {
    class AuthAddressDialog;
}

namespace spdlog {
   class logger;
}


class AuthAddressDialog : public QDialog
{
Q_OBJECT

public:
   AuthAddressDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<AuthAddressManager>& authAddressManager
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<ApplicationSettings> &, QWidget* parent = nullptr);
   ~AuthAddressDialog() override;

   void setAddressToVerify(const QString &addr);
   void setBsClient(BsClient *bsClient);

signals:
   void askForConfirmation(const QString &address, double txAmount);

private slots:
   void resizeTreeViewColumns();
   void adressSelected(const QItemSelection &selected, const QItemSelection &deselected);

   void createAddress();
   void revokeSelectedAddress();
   void submitSelectedAddress();
   void setDefaultAddress();

   void onModelReset();
   void onAddressListUpdated();
   void onAddressStateChanged(const QString &addr, const QString &state);

   void onAuthMgrError(const QString &details);
   void onAuthMgrInfo(const QString &text);

   void onAuthAddressConfirmationRequired(float validationAmount);

   void ConfirmAuthAddressSubmission();

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
   std::unique_ptr<Ui::AuthAddressDialog> ui_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<AuthAddressManager>    authAddressManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<ApplicationSettings>   settings_;
   AuthAddressViewModel                *  model_;
   bs::Address                            defaultAddr_;
   QPointer<BsClient>                     bsClient_;

   bs::Address                            lastSubmittedAddress_;

   bool  unconfirmedExists_ = false;
};

#endif // __AUTH_ADDRESS_DIALOG_H__
