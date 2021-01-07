/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __AUTH_ADDRESS_DIALOG_H__
#define __AUTH_ADDRESS_DIALOG_H__

#include "AuthAddressManager.h"
#include "BinaryData.h"
#include "BsClient.h"
#include "ValidityFlag.h"

#include <memory>

#include <QDialog>
#include <QPointer>

class AssetManager;
class AuthAdressControlProxyModel;
class QItemSelection;

namespace Ui {
    class AuthAddressDialog;
}

namespace bs {
   struct TradeSettings;
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
   void setBsClient(const std::weak_ptr<BsClient>& bsClient);

signals:
   void askForConfirmation(const QString &address, double txAmount);

private slots:
   void resizeTreeViewColumns();
   void adressSelected();

   void createAddress();
   void revokeSelectedAddress();
   void submitSelectedAddress();
   void setDefaultAddress();

   void onModelReset();
   void onAddressStateChanged(const QString &addr, int state);

   void onAuthMgrError(const QString &details);
   void onAuthMgrInfo(const QString &text);

   void onAuthVerifyTxSent();
   void onUpdateSelection(int row);
   void copySelectedToClipboard();

protected:
   void showEvent(QShowEvent *) override;
   void showError(const QString &text, const QString &details = {});
   void showInfo(const QString &title, const QString &text);
   bool eventFilter(QObject* sender, QEvent* event) override;

private:
   bs::Address GetSelectedAddress() const;
   bool unsubmittedExist() const;
   void updateUnsubmittedState();
   void saveAddressesNumber();

   void setLastSubmittedAddress(const bs::Address &address);
   void updateEnabledStates();

private:
   std::unique_ptr<Ui::AuthAddressDialog> ui_;
   std::shared_ptr<spdlog::logger>        logger_;
   std::shared_ptr<AuthAddressManager>    authAddressManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<ApplicationSettings>   settings_;
   QPointer<AuthAdressControlProxyModel>  model_;
   bs::Address                            defaultAddr_;
   std::weak_ptr<BsClient>                bsClient_;
   ValidityFlag                           validityFlag_;

   bs::Address                            lastSubmittedAddress_{};

   bool  unconfirmedExists_ = false;
};

#endif // __AUTH_ADDRESS_DIALOG_H__
