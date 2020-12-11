/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
#include "SignerDefs.h"
#include "SignerUiDefs.h"
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
class AddressDetailDialog;
class AddressListModel;
class AddressSortFilterModel;
class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class ConnectionManager;
class QAction;
class QMenu;
class RootWalletPropertiesDialog;
class SignContainer;
class WalletNode;
class WalletsViewModel;

class WalletsWidget : public TabWithShortcut
{
Q_OBJECT

public:
   WalletsWidget(QWidget* parent = nullptr );
   ~WalletsWidget() override;

   [[deprecated]] void init(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ApplicationSettings> &
      , const std::shared_ptr<ConnectionManager> &connectionManager
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<ArmoryConnection> &);

   void init(const std::shared_ptr<spdlog::logger> &logger);

   void setUsername(const QString& username);

   WalletNode *getSelectedNode() const;
   std::vector<bs::sync::WalletInfo> getSelectedWallets() const;
   std::vector<bs::sync::WalletInfo> getFirstWallets() const;
   bs::sync::WalletInfo getSelectedHdWallet() const;

   void CreateNewWallet();
   void ImportNewWallet();
   void ImportHwWallet();

   void onNewBlock(unsigned int blockNum);
   void onHDWallet(const bs::sync::WalletInfo &);
   void onHDWalletDetails(const bs::sync::HDWalletData &);
   void onAddresses(const std::vector<bs::sync::Address> &);
   void onAddressComments(const std::string &walletId
      , const std::map<bs::Address, std::string> &);
   void onWalletBalance(const bs::sync::WalletBalanceData &);
   void onLedgerEntries(const std::string &filter, uint32_t totalPages
      , uint32_t curPage, uint32_t curBlock, const std::vector<bs::TXEntry> &);
   void onTXDetails(const std::vector<bs::sync::TXWalletDetails> &);

   void shortcutActivated(ShortcutType s) override;

public slots:
   void onNewWallet();

private:
   void InitWalletsView(const std::string& defaultWalletId);

   void showInfo(bool report, const QString &title, const QString &text) const;
   void showError(const QString &text) const;

   int getUIFilterSettings() const;
   void updateAddressFilters(int filterSettings);
   bool applyPreviousSelection();
   bool filterBtcOnly() const;

signals:
   void showContextMenu(QMenu *, QPoint); // deprecated
   void newWalletCreationRequest();
   void needHDWalletDetails(const std::string &walletId);
   void needWalletBalances(const std::string &walletId);
   void needUTXOs(const std::string& id, const std::string& walletId
      , bool confOnly = false, bool swOnly = false);
   void needExtAddresses(const std::string &walletId);
   void needIntAddresses(const std::string &walletId);
   void needUsedAddresses(const std::string &walletId);
   void needAddrComments(const std::string &walletId, const std::vector<bs::Address> &);
   void setAddrComment(const std::string &walletId, const bs::Address &
      , const std::string &comment);
   void needLedgerEntries(const std::string &filter);
   void needTXDetails(const std::vector<bs::sync::TXWallet> &, bool useCache
      , const bs::Address &);
   void needWalletDialog(bs::signer::ui::GeneralDialogType, const std::string& rootId);

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
   RootWalletPropertiesDialog *  rootDlg_{ nullptr };
   QAction  *  actCopyAddr_ = nullptr;
   QAction  *  actEditComment_ = nullptr;
   QAction  *  actRevokeSettl_ = nullptr;
   //QAction  *  actDeleteWallet_ = nullptr;
   bs::Address curAddress_;
   std::shared_ptr<bs::sync::Wallet>   curWallet_;
   std::string    curWalletId_;
   std::string    curComment_;
   unsigned int   revokeReqId_ = 0;
   QString username_;
   std::vector<bs::sync::WalletInfo>   prevSelectedWallets_;
   int prevSelectedWalletRow_{-1};
   int prevSelectedAddressRow_{-1};
   QPoint walletsScrollPos_;
   QPoint addressesScrollPos_;
   std::unordered_map<std::string, AddressDetailDialog *>   addrDetDialogs_;
};

#endif // __WALLETS_WIDGET_H__
