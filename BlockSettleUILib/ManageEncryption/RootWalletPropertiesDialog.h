/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
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
#include "Wallets/SignerDefs.h"
#include "Wallets/SignerUiDefs.h"
#include "Wallets/QWalletInfo.h"

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
class HeadlessContainer;
class WalletsViewModel;
class ConnectionManager;


class RootWalletPropertiesDialog : public QDialog
{
Q_OBJECT

public:
   RootWalletPropertiesDialog(const std::shared_ptr<spdlog::logger> &logger
      , const bs::sync::WalletInfo &, WalletsViewModel *walletsModel
      , QWidget* parent = nullptr);
   ~RootWalletPropertiesDialog() override;

   void onHDWalletDetails(const bs::sync::HDWalletData&);
   void onWalletBalances(const bs::sync::WalletBalanceData&);
   void onSpendableUTXOs();
   void walletDeleted(const std::string& rootId);

signals:
   void startRescan(std::string walletId);
   void needHDWalletDetails(const std::string &walletId);
   void needWalletBalances(const std::string &walletId);
   void needUTXOs(const std::string& id, const std::string& walletId
      , bool confOnly = false, bool swOnly = false);
   void needWalletDialog(bs::signer::ui::GeneralDialogType, const std::string& rootId);

private slots:
   void onDeleteWallet();
   void onBackupWallet();
   void onChangePassword();
   void onHDWalletInfo(unsigned int id, const bs::hd::WalletInfo &walletInfo);
   void onWalletSelected();
   void onRescanBlockchain();
   void onModelReset();

private:
   void updateWalletDetails(const bs::sync::WalletInfo &);

private:
   std::unique_ptr<Ui::WalletPropertiesDialog>  ui_;
   bs::sync::WalletInfo    wallet_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   bs::hd::WalletInfo                  walletInfo_;
   std::shared_ptr<HeadlessContainer>  signingContainer_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::shared_ptr<AssetManager>       assetMgr_;
   std::shared_ptr<spdlog::logger>     logger_;
   CurrentWalletFilter                 *walletFilter_;
   unsigned int                        infoReqId_ = 0;
   std::map<unsigned int, std::string> createCCWalletReqs_;
};

#endif // __ROOT_WALLET_PROPERTIES_DIALOG_H__
