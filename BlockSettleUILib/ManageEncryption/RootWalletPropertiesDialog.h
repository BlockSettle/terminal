/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ROOT_WALLET_PROPERTIES_DIALOG_H__
#define __ROOT_WALLET_PROPERTIES_DIALOG_H__

#include <QDialog>
#include <memory>
#include "BinaryData.h"
#include "QWalletInfo.h"

namespace Ui {
    class WalletPropertiesDialog;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class WalletsManager;
      class Wallet;
   }
}
class ArmoryConnection;
class ApplicationSettings;
class AssetManager;
class CurrentWalletFilter;
class SignContainer;
class WalletsViewModel;
class ConnectionManager;


class RootWalletPropertiesDialog : public QDialog
{
Q_OBJECT

public:
   RootWalletPropertiesDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::sync::hd::Wallet> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<SignContainer> &
      , WalletsViewModel *walletsModel
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<AssetManager> &, QWidget* parent = nullptr);
   ~RootWalletPropertiesDialog() override;

private slots:
   void onDeleteWallet();
   void onBackupWallet();
   void onChangePassword();
   void onHDWalletInfo(unsigned int id, const bs::hd::WalletInfo &walletInfo);
   void onWalletSelected();
   void onRescanBlockchain();
   void onModelReset();

private:
   void updateWalletDetails(const std::shared_ptr<bs::sync::hd::Wallet> &);
   void updateWalletDetails(const std::shared_ptr<bs::sync::Wallet> &);

private:
   std::unique_ptr<Ui::WalletPropertiesDialog>  ui_;
   std::shared_ptr<bs::sync::hd::Wallet>        wallet_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   bs::hd::WalletInfo                  walletInfo_;
   std::shared_ptr<SignContainer>      signingContainer_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::shared_ptr<AssetManager>       assetMgr_;
   std::shared_ptr<spdlog::logger>     logger_;
   CurrentWalletFilter                 *walletFilter_;
   unsigned int                        infoReqId_ = 0;
   std::map<unsigned int, std::string> createCCWalletReqs_;
};

#endif // __ROOT_WALLET_PROPERTIES_DIALOG_H__
