/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __WALLETS_WIDGET_H__
#define __WALLETS_WIDGET_H__

#include <memory>
#include <unordered_map>
#include <QWidget>
#include <QItemSelection>
#include "Address.h"
#include "TabWithShortcut.h"
#include "BSErrorCode.h"
#include "BSErrorCodeStrings.h"


namespace Ui {
    class WalletsWidget;
}
namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Wallet;
      }
      class Wallet;
      class WalletsManager;
   }
}
class AddressListModel;
class AddressSortFilterModel;
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class ConnectionManager;
class QAction;
class QMenu;
class SignContainer;
class WalletNode;
class WalletsViewModel;

class WalletsWidget : public TabWithShortcut
{
Q_OBJECT

public:
   WalletsWidget(QWidget* parent = nullptr );
   ~WalletsWidget() override;

   void init(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<ArmoryConnection> &);

   void setUsername(const QString& username);

   WalletNode *getSelectedNode() const;
   std::vector<std::shared_ptr<bs::sync::Wallet>> getSelectedWallets() const;
   std::vector<std::shared_ptr<bs::sync::Wallet>> getFirstWallets() const;
   std::shared_ptr<bs::sync::hd::Wallet> getSelectedHdWallet() const;

   bool CreateNewWallet(bool report = true);
   bool ImportNewWallet(bool report = true);

   void shortcutActivated(ShortcutType s) override;

public slots:
   void onNewWallet();

private:
   void InitWalletsView(const std::string& defaultWalletId);

   void showInfo(bool report, const QString &title, const QString &text) const;
   void showError(const QString &text) const;

   int getUIFilterSettings() const;
   void updateAddressFilters(int filterSettings);
   void keepSelection();
   bool filterBtcOnly() const;

signals:
   void showContextMenu(QMenu *, QPoint);

private slots:
   void showWalletProperties(const QModelIndex& index);
   void showSelectedWalletProperties();
   void showAddressProperties(const QModelIndex& index);
   void updateAddresses();
   void onAddressContextMenu(const QPoint &);
   //void onWalletContextMenu(const QPoint &);
   void onCopyAddress();
   void onEditAddrComment();
   void onRevokeSettlement();
   void onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result);
   //void onDeleteWallet();
   void onFilterSettingsChanged();
   void onEnterKeyInAddressesPressed(const QModelIndex &index);
   void onEnterKeyInWalletsPressed(const QModelIndex &index);
   void onShowContextMenu(QMenu *, QPoint);
   void onWalletBalanceChanged(std::string);
   void treeViewAddressesSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
   void treeViewAddressesLayoutChanged();
   void scrollChanged();
   void onWalletsSynchronized();

private:
   std::unique_ptr<Ui::WalletsWidget> ui_;

   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<AuthAddressManager>    authMgr_;
   std::shared_ptr<ArmoryConnection>      armory_;
   WalletsViewModel        *  walletsModel_;
   AddressListModel        *  addressModel_;
   AddressSortFilterModel  *  addressSortFilterModel_;
   QAction  *  actCopyAddr_ = nullptr;
   QAction  *  actEditComment_ = nullptr;
   QAction  *  actRevokeSettl_ = nullptr;
   //QAction  *  actDeleteWallet_ = nullptr;
   bs::Address curAddress_;
   std::shared_ptr<bs::sync::Wallet>   curWallet_;
   unsigned int   revokeReqId_ = 0;
   QString username_;
   std::vector<std::shared_ptr<bs::sync::Wallet>>  prevSelectedWallets_;
   int prevSelectedWalletRow_;
   int prevSelectedAddressRow_;
   QPoint walletsScrollPos_;
   QPoint addressesScrollPos_;
};

#endif // __WALLETS_WIDGET_H__
