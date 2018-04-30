#ifndef __WALLETS_WIDGET_H__
#define __WALLETS_WIDGET_H__

#include <memory>
#include <QWidget>
#include "WalletsManager.h"


namespace Ui {
    class WalletsWidget;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
}
class AddressListModel;
class AddressSortFilterModel;
class ApplicationSettings;
class AssetManager;
class AuthAddressManager;
class QAction;
class SignContainer;
class WalletImporter;
class WalletsViewModel;


class WalletsWidget : public QWidget
{
Q_OBJECT

public:
   WalletsWidget(QWidget* parent = nullptr );
   ~WalletsWidget() override = default;

   void init(const std::shared_ptr<WalletsManager> &, const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<ApplicationSettings> &, const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<AuthAddressManager> &);

   std::vector<WalletsManager::wallet_gen_type> GetSelectedWallets() const;

   bool CreateNewWallet(bool primary, bool report = true);
   bool ImportNewWallet(bool primary, bool report = true);

private:
   void InitWalletsView(const std::string& defaultWalletId);

   void showInfo(bool report, const QString &title, const QString &text) const;
   void showError(const QString &text) const;

   int getUIFilterSettings() const;
   void updateAddressFilters(int filterSettings);

private slots:
   void showWalletProperties(const QModelIndex& index);
   void showSelectedWalletProperties();
   void showAddressProperties(const QModelIndex& index);
   void updateAddresses();
   void onAddressContextMenu(const QPoint &);
   void onWalletContextMenu(const QPoint &);
   void onImportComplete(const std::string &walletId);
   void onNewWallet();
   void onCopyAddress();
   void onEditAddrComment();
   void onRevokeSettlement();
   void onTXSigned(unsigned int id, BinaryData signedTX, std::string error);
   void onDeleteWallet();

   void onFilterSettingsChanged();

private:
   Ui::WalletsWidget* ui;

   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signingContainer_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<AssetManager>          assetManager_;
   std::shared_ptr<AuthAddressManager>    authMgr_;
   WalletsViewModel        *  walletsModel_;
   AddressListModel        *  addressModel_;
   AddressSortFilterModel  *  addressSortFilterModel_;
   std::unordered_map<std::string, std::shared_ptr<WalletImporter>>  walletImporters_;
   QAction  *  actCopyAddr_ = nullptr;
   QAction  *  actEditComment_ = nullptr;
   QAction  *  actRevokeSettl_ = nullptr;
   QAction  *  actDeleteWallet_ = nullptr;
   bs::Address curAddress_;
   std::shared_ptr<bs::Wallet>   curWallet_;
   unsigned int   revokeReqId_ = 0;
};

bool WalletBackupAndVerify(const std::shared_ptr<bs::hd::Wallet> &, const std::shared_ptr<SignContainer> &
   , QWidget *parent);

#endif // __WALLETS_WIDGET_H__
