/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "WalletsWidget.h"
#include "ui_WalletsWidget.h"

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QModelIndex>
#include <QSortFilterProxyModel>
#include <QScrollBar>
#include <QItemSelectionModel>

#include "AddressDetailDialog.h"
#include "AddressListModel.h"
#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "BSMessageBox.h"
#include "NewAddressDialog.h"
#include "NewWalletDialog.h"
#include "SelectWalletDialog.h"
#include "SignContainer.h"
#include "WalletsViewModel.h"
#include "WalletWarningDialog.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "TreeViewWithEnterKey.h"
#include "ManageEncryption/RootWalletPropertiesDialog.h"

#include "SignerUiDefs.h"

class AddressSortFilterModel : public QSortFilterProxyModel
{
public:
   AddressSortFilterModel(QObject* parent) : QSortFilterProxyModel(parent) {}

   enum FilterOption
   {
      NoFilter = 0x00,
      HideEmpty = 0x01,
      HideInternal = 0x02,
      HideUsedEmpty = 0x04,
      HideExternal = 0x08
   };
   Q_DECLARE_FLAGS(Filter, FilterOption)

   bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override
   {
      const int txCount = sourceModel()->data(sourceModel()->index(
         source_row, AddressListModel::ColumnTxCount, source_parent)).toInt();
      const double balance = QLocale().toDouble(sourceModel()->data(sourceModel()->index(
         source_row, AddressListModel::ColumnBalance, source_parent)).toString());
      const bool isExternal = sourceModel()->data(sourceModel()->index(
            source_row, AddressListModel::ColumnAddress, source_parent),
         AddressListModel::IsExternalRole).toBool();

      if (filterMode_ & HideInternal) {
         if (txCount == 0 && qFuzzyIsNull(balance) && !isExternal) {
            return false;
         }
      }

      if (filterMode_ & HideExternal) {
         if (txCount == 0 && qFuzzyIsNull(balance) && isExternal) {
            return false;
         }
      }

      if (filterMode_ & HideEmpty) {
         if (qFuzzyIsNull(balance)) {
            return false;
         }
      }

      if (filterMode_ & HideUsedEmpty) {
         if (txCount != 0) {
            if (qFuzzyIsNull(balance)) {
               return false;
            }
         }
      }

      //TODO: Filter for change addresses not implemented yet
      return true;
   }

   bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
   {
      if (left.column() == AddressListModel::ColumnBalance && right.column() == AddressListModel::ColumnBalance) {
         QVariant leftData = sourceModel()->data(left, AddressListModel::SortRole);
         QVariant rightData = sourceModel()->data(right, AddressListModel::SortRole);

         if (leftData != rightData) {
            if (leftData.type() == QVariant::String && rightData.type() == QVariant::String) {
               bool leftConverted = false;
               double leftDoubleValue = leftData.toString().toDouble(&leftConverted);

               bool rightConverted = false;
               double rightDoubleValue = rightData.toString().toDouble(&rightConverted);

               if (leftConverted && rightConverted) {
                  return (leftDoubleValue < rightDoubleValue);
               }
            }
            else {
               return (leftData < rightData);
            }
         } else {
            const QModelIndex lTxnIndex = sourceModel()->index(left.row(), AddressListModel::ColumnTxCount);
            const QModelIndex rTxnIndex = sourceModel()->index(right.row(), AddressListModel::ColumnTxCount);
            const auto lData = sourceModel()->data(lTxnIndex, AddressListModel::SortRole);
            const auto rData = sourceModel()->data(rTxnIndex, AddressListModel::SortRole);
            if (lData != rData) {
               return (lData < rData);
            }
         }
      }

      return QSortFilterProxyModel::lessThan(left, right);
   }

   void setFilter(const Filter &filter) {
      filterMode_ = filter;
      invalidate();
   }

private:
   Filter filterMode_ = NoFilter;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(AddressSortFilterModel::Filter)

WalletsWidget::WalletsWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui_(new Ui::WalletsWidget())
   , walletsManager_(nullptr)
   , walletsModel_(nullptr)
   , addressModel_(nullptr)
   , addressSortFilterModel_(nullptr)
{
   ui_->setupUi(this);

   ui_->gridLayout->setRowStretch(0, 1);
   ui_->gridLayout->setRowStretch(1, 2);

   actCopyAddr_ = new QAction(tr("&Copy Address"), this);
   connect(actCopyAddr_, &QAction::triggered, this, &WalletsWidget::onCopyAddress);

   actEditComment_ = new QAction(tr("&Edit Comment"));
   connect(actEditComment_, &QAction::triggered, this, &WalletsWidget::onEditAddrComment);

   actRevokeSettl_ = new QAction(tr("&Revoke Settlement"));
   connect(actRevokeSettl_, &QAction::triggered, this, &WalletsWidget::onRevokeSettlement);

//   actDeleteWallet_ = new QAction(tr("&Delete Permanently"));
//   connect(actDeleteWallet_, &QAction::triggered, this, &WalletsWidget::onDeleteWallet);

   connect(ui_->treeViewAddresses, &TreeViewWithEnterKey::enterKeyPressed,
           this, &WalletsWidget::onEnterKeyInAddressesPressed);
   connect(ui_->treeViewWallets, &WalletsTreeView::enterKeyPressed,
           this, &WalletsWidget::onEnterKeyInWalletsPressed);
   connect(this, &WalletsWidget::showContextMenu, this, &WalletsWidget::onShowContextMenu, Qt::QueuedConnection);

   ui_->treeViewWallets->setEnableDeselection(false);
}

WalletsWidget::~WalletsWidget() = default;

void WalletsWidget::init(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::sync::WalletsManager> &manager
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ApplicationSettings> &applicationSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<AssetManager> &assetMgr
   , const std::shared_ptr<AuthAddressManager> &authMgr
   , const std::shared_ptr<ArmoryConnection> &armory)
{
   logger_ = logger;
   walletsManager_ = manager;
   signingContainer_ = container;
   appSettings_ = applicationSettings;
   assetManager_ = assetMgr;
   authMgr_ = authMgr;
   armory_ = armory;
   connectionManager_ = connectionManager;

   // signingContainer_ might be null if user rejects remote signer key
   if (signingContainer_) {
      connect(signingContainer_.get(), &SignContainer::TXSigned, this, &WalletsWidget::onTXSigned);
   }

   const auto &defWallet = walletsManager_->getDefaultWallet();
   InitWalletsView(defWallet ? defWallet->walletId() : std::string{});
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletImportFinished, [this] { walletsModel_->LoadWallets(); });

   auto filter = appSettings_->get<int>(ApplicationSettings::WalletFiltering);

   ui_->pushButtonEmpty->setChecked(filter & AddressSortFilterModel::HideEmpty);
   ui_->pushButtonInternal->setChecked(filter & AddressSortFilterModel::HideInternal);
   ui_->pushButtonExternal->setChecked(filter & AddressSortFilterModel::HideExternal);
   ui_->pushButtonUsed->setChecked(filter & AddressSortFilterModel::HideUsedEmpty);

   updateAddressFilters(filter);

   for (auto button : {ui_->pushButtonEmpty, ui_->pushButtonInternal,
      ui_->pushButtonExternal, ui_->pushButtonUsed}) {
         connect(button, &QPushButton::toggled, this, &WalletsWidget::onFilterSettingsChanged);
   }

   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsSynchronized, this, &WalletsWidget::onWalletsSynchronized, Qt::QueuedConnection);
}

void WalletsWidget::init(const std::shared_ptr<spdlog::logger> &logger)
{
   logger_ = logger;

//   const auto &defWallet = walletsManager_->getDefaultWallet();
//   InitWalletsView(defWallet ? defWallet->walletId() : std::string{});

/*   auto filter = appSettings_->get<int>(ApplicationSettings::WalletFiltering);

   ui_->pushButtonEmpty->setChecked(filter & AddressSortFilterModel::HideEmpty);
   ui_->pushButtonInternal->setChecked(filter & AddressSortFilterModel::HideInternal);
   ui_->pushButtonExternal->setChecked(filter & AddressSortFilterModel::HideExternal);
   ui_->pushButtonUsed->setChecked(filter & AddressSortFilterModel::HideUsedEmpty);

   updateAddressFilters(filter);*/

   InitWalletsView({});
   for (auto button : { ui_->pushButtonEmpty, ui_->pushButtonInternal,
      ui_->pushButtonExternal, ui_->pushButtonUsed }) {
      connect(button, &QPushButton::toggled, this, &WalletsWidget::onFilterSettingsChanged);
   }
}


void WalletsWidget::setUsername(const QString& username)
{
   username_ = username;
}

void WalletsWidget::InitWalletsView(const std::string& defaultWalletId)
{
   if (walletsManager_ && signingContainer_) {
      walletsModel_ = new WalletsViewModel(walletsManager_, defaultWalletId, signingContainer_, ui_->treeViewWallets);
   }
   else {
      walletsModel_ = new WalletsViewModel(defaultWalletId, ui_->treeViewWallets);
   }
   connect(walletsModel_, &WalletsViewModel::needHDWalletDetails, this, &WalletsWidget::needHDWalletDetails);
   connect(walletsModel_, &WalletsViewModel::needWalletBalances, this, &WalletsWidget::needWalletBalances);

   ui_->treeViewWallets->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewWallets->setModel(walletsModel_);
   ui_->treeViewWallets->setFocus(Qt::ActiveWindowFocusReason);
   ui_->treeViewWallets->setItemsExpandable(true);
   ui_->treeViewWallets->setRootIsDecorated(true);
   ui_->treeViewWallets->setExpandsOnDoubleClick(false);
   // show the column as per BST-1520
   //ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnID));
   if (walletsManager_ && signingContainer_) {
      walletsModel_->LoadWallets();
   }

   connect(ui_->walletPropertiesButton, &QPushButton::clicked, this, &WalletsWidget::showSelectedWalletProperties);
   connect(ui_->createWalletButton, &QPushButton::clicked, this, &WalletsWidget::onNewWallet);
   connect(ui_->treeViewWallets, &QTreeView::doubleClicked, this, &WalletsWidget::showWalletProperties);
   connect(ui_->treeViewAddresses, &QTreeView::doubleClicked, this, &WalletsWidget::showAddressProperties);

   ui_->treeViewAddresses->setContextMenuPolicy(Qt::CustomContextMenu);
   ui_->treeViewWallets->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui_->treeViewAddresses, &QTreeView::customContextMenuRequested, this, &WalletsWidget::onAddressContextMenu);
   //connect(ui_->treeViewWallets, &QTreeView::customContextMenuRequested, this, &WalletsWidget::onWalletContextMenu);

   // No need to connect to wallet manager in AddressListModel explicitly in this case
   // so just put nullptr pointer in function
   addressModel_ = new AddressListModel(nullptr, this);
   connect(addressModel_, &AddressListModel::needExtAddresses, this, &WalletsWidget::needExtAddresses);
   connect(addressModel_, &AddressListModel::needIntAddresses, this, &WalletsWidget::needIntAddresses);
   connect(addressModel_, &AddressListModel::needUsedAddresses, this, &WalletsWidget::needUsedAddresses);
   connect(addressModel_, &AddressListModel::needAddrComments, this, &WalletsWidget::needAddrComments);

   addressSortFilterModel_ = new AddressSortFilterModel(this);
   addressSortFilterModel_->setSourceModel(addressModel_);
   addressSortFilterModel_->setSortRole(AddressListModel::SortRole);

   ui_->treeViewAddresses->setUniformRowHeights(true);
   ui_->treeViewAddresses->setModel(addressSortFilterModel_);
   ui_->treeViewAddresses->sortByColumn(2, Qt::DescendingOrder);
   ui_->treeViewAddresses->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
   ui_->treeViewAddresses->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   updateAddresses();
   connect(ui_->treeViewWallets->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &WalletsWidget::updateAddresses);
   connect(walletsModel_, &WalletsViewModel::updateAddresses, this, &WalletsWidget::updateAddresses);
   if (walletsManager_) {
      connect(walletsManager_.get(), &bs::sync::WalletsManager::walletBalanceUpdated, this, &WalletsWidget::onWalletBalanceChanged, Qt::QueuedConnection);
   }
   connect(ui_->treeViewAddresses->model(), &QAbstractItemModel::layoutChanged, this, &WalletsWidget::treeViewAddressesLayoutChanged);
   connect(ui_->treeViewAddresses->selectionModel(), &QItemSelectionModel::selectionChanged, this, &WalletsWidget::treeViewAddressesSelectionChanged);

   connect(ui_->treeViewWallets->horizontalScrollBar(), &QScrollBar::valueChanged, this, &WalletsWidget::scrollChanged);
   connect(ui_->treeViewWallets->verticalScrollBar(), &QScrollBar::valueChanged, this, &WalletsWidget::scrollChanged);
   connect(ui_->treeViewAddresses->horizontalScrollBar(), &QScrollBar::valueChanged, this, &WalletsWidget::scrollChanged);
   connect(ui_->treeViewAddresses->verticalScrollBar(), &QScrollBar::valueChanged, this, &WalletsWidget::scrollChanged);
}

WalletNode *WalletsWidget::getSelectedNode() const
{
   auto indices = ui_->treeViewWallets->selectionModel()->selectedIndexes();
   if (!indices.isEmpty()) {
      return walletsModel_->getNode(indices.first());
   }
   return nullptr;
}

std::vector<bs::sync::WalletInfo> WalletsWidget::getSelectedWallets() const
{
   const auto node = getSelectedNode();
   return node ? node->wallets() : std::vector<bs::sync::WalletInfo>{};
}

bs::sync::WalletInfo WalletsWidget::getSelectedHdWallet() const
{
   const auto node = getSelectedNode();
   return node ? node->hdWallet() : bs::sync::WalletInfo{};
}

std::vector<bs::sync::WalletInfo> WalletsWidget::getFirstWallets() const
{
   if (walletsModel_->rowCount()) {
      return walletsModel_->getWallets(walletsModel_->index(0, 0));
   } else {
      return {};
   }
}

void WalletsWidget::onNewBlock(unsigned int blockNum)
{
   for (const auto &dlg : addrDetDialogs_) {
      dlg.second->onNewBlock(blockNum);
   }
}

void WalletsWidget::onHDWallet(const bs::sync::WalletInfo &wi)
{
   wallets_.insert(wi);
   walletsModel_->onHDWallet(wi);
}

void WalletsWidget::onHDWalletDetails(const bs::sync::HDWalletData &hdWallet)
{
   walletDetails_[hdWallet.id] = hdWallet;
   if (rootDlg_) {
      rootDlg_->onHDWalletDetails(hdWallet);
   }
   walletsModel_->onHDWalletDetails(hdWallet);

   for (int i = 0; i < walletsModel_->rowCount(); ++i) {
      ui_->treeViewWallets->expand(walletsModel_->index(i, 0));
      // Expand XBT leaves
      ui_->treeViewWallets->expand(walletsModel_->index(0, 0
         , walletsModel_->index(i, 0)));
   }

   //TODO: keep wallets treeView selection
}

void WalletsWidget::onGenerateAddress(bool isActive)
{
   if (wallets_.empty()) {
      //TODO: invoke wallet create dialog
      return;
   }

   std::string selWalletId = curWalletId_;
   if (!isActive || selWalletId.empty()) {
      SelectWalletDialog selectWalletDialog(curWalletId_, this);
      for (const auto& wallet : wallets_) {
         selectWalletDialog.onHDWallet(wallet);
      }
      for (const auto& wallet : walletDetails_) {
         selectWalletDialog.onHDWalletDetails(wallet.second);
      }
      for (const auto& bal : walletBalances_) {
         selectWalletDialog.onWalletBalances(bal.second);
      }
      selectWalletDialog.exec();

      if (selectWalletDialog.result() == QDialog::Rejected) {
         return;
      }
      else {
         selWalletId = selectWalletDialog.getSelectedWallet();
      }
   }
   bs::sync::WalletInfo selWalletInfo;
   auto itWallet = walletDetails_.find(selWalletId);
   if (itWallet == walletDetails_.end()) {
      const auto& findLeaf = [selWalletId, wallets = walletDetails_]() -> bs::sync::WalletInfo
      {
         for (const auto& hdWallet : wallets) {
            for (const auto& grp : hdWallet.second.groups) {
               for (const auto& leaf : grp.leaves) {
                  for (const auto& id : leaf.ids) {
                     if (id == selWalletId) {
                        bs::sync::WalletInfo wi;
                        wi.name = leaf.name;
                        wi.ids = leaf.ids;
                        return wi;
                     }
                  }
               }
            }
         }
         return {};
      };
      selWalletInfo = findLeaf();
      if (selWalletInfo.ids.empty()) {
         logger_->error("[{}] no leaf found for {}", __func__, selWalletId);
         return;
      }
   }
   else {
      selWalletInfo.name = itWallet->second.name;
   }

   if (newAddrDlg_) {
      logger_->error("[{}] new address dialog already created", __func__);
      return;
   }
   newAddrDlg_ = new NewAddressDialog(selWalletInfo, this);
   emit createExtAddress(selWalletId);
   connect(newAddrDlg_, &QDialog::finished, [this](int) {
      newAddrDlg_->deleteLater();
      newAddrDlg_ = nullptr;
   });
   newAddrDlg_->exec();
}

void WalletsWidget::onAddresses(const std::string &walletId
   , const std::vector<bs::sync::Address> &addrs)
{
   if (newAddrDlg_) {
      newAddrDlg_->onAddresses(walletId, addrs);
   }
   addressModel_->onAddresses(walletId, addrs);
}

void WalletsWidget::onLedgerEntries(const std::string &filter, uint32_t totalPages
   , uint32_t curPage, uint32_t curBlock, const std::vector<bs::TXEntry> &entries)
{
   const auto &itDlg = addrDetDialogs_.find(filter);
   if (itDlg != addrDetDialogs_.end()) {
      itDlg->second->onLedgerEntries(curBlock, entries);
   }
}

void WalletsWidget::onTXDetails(const std::vector<bs::sync::TXWalletDetails> &details)
{
   for (const auto &dlg : addrDetDialogs_) {
      dlg.second->onTXDetails(details);
   }
}

void WalletsWidget::onAddressComments(const std::string &walletId
   , const std::map<bs::Address, std::string> &comments)
{
   addressModel_->onAddressComments(walletId, comments);
}

void WalletsWidget::onWalletBalance(const bs::sync::WalletBalanceData &wbd)
{
   walletBalances_[wbd.id] = wbd;
   if (rootDlg_) {
      rootDlg_->onWalletBalances(wbd);
   }
   walletsModel_->onWalletBalances(wbd);
   addressModel_->onAddressBalances(wbd.id, wbd.addrBalances);
}

void WalletsWidget::showSelectedWalletProperties()
{
   auto indexes = ui_->treeViewWallets->selectionModel()->selectedIndexes();
   if (!indexes.isEmpty())
   {
      showWalletProperties(indexes.first());
   }
}

void WalletsWidget::showWalletProperties(const QModelIndex& index)
{
   auto node = walletsModel_->getNode(index);
   if (node == nullptr) {
      return;
   }

   while (node->parent()->type() != WalletNode::Type::Root) {
      node = node->parent();
   }

   const auto &hdWallet = node->hdWallet();
   if (!(*hdWallet.ids.cbegin()).empty()) {
      if (walletsManager_ && armory_ && signingContainer_ && appSettings_ && connectionManager_ && assetManager_) {
         RootWalletPropertiesDialog(logger_, hdWallet, walletsManager_, armory_, signingContainer_
            , walletsModel_, appSettings_, connectionManager_, assetManager_, this).exec();
      }
      else {
         rootDlg_ = new RootWalletPropertiesDialog(logger_, hdWallet, walletsModel_, this);
         connect(rootDlg_, &RootWalletPropertiesDialog::needHDWalletDetails, this, &WalletsWidget::needHDWalletDetails);
         connect(rootDlg_, &RootWalletPropertiesDialog::needWalletBalances, this, &WalletsWidget::needWalletBalances);
         connect(rootDlg_, &RootWalletPropertiesDialog::needUTXOs, this, &WalletsWidget::needUTXOs);
         connect(rootDlg_, &RootWalletPropertiesDialog::needWalletDialog, this, &WalletsWidget::needWalletDialog);
         connect(rootDlg_, &QDialog::finished, [this](int) {
            rootDlg_->deleteLater();
            rootDlg_ = nullptr;
         });
         rootDlg_->exec();
      }
   }
}

void WalletsWidget::showAddressProperties(const QModelIndex& index)
{
   auto sourceIndex = addressSortFilterModel_->mapToSource(index);
   const auto &walletId = addressModel_->data(sourceIndex, AddressListModel::WalletIdRole).toString().toStdString();
   if (walletsManager_ && armory_) {
      const auto &wallet = walletsManager_->getWalletById(walletId);
      if (!wallet || (wallet->type() == bs::core::wallet::Type::Authentication)) {
         return;
      }

      const auto &addresses = wallet->getUsedAddressList();
      const size_t addrIndex = addressModel_->data(sourceIndex, AddressListModel::AddrIndexRole).toUInt();
      const auto address = (addrIndex < addresses.size()) ? addresses[addrIndex] : bs::Address();

      wallet->onBalanceAvailable([this, address, wallet] {
         auto dialog = new AddressDetailDialog(address, wallet, walletsManager_, armory_, logger_, this);
         QMetaObject::invokeMethod(this, [dialog] { dialog->exec(); });
      });
   }
   else {
      const auto &addrStr = addressModel_->data(sourceIndex, AddressListModel::AddressRole).toString().toStdString();
      const auto &ledgerFilter = walletId + "." + addrStr;
      emit needLedgerEntries(ledgerFilter);
      const auto wltType = static_cast<bs::core::wallet::Type>(addressModel_->data(sourceIndex, AddressListModel::WalletTypeRole).toInt());
      const auto txn = addressModel_->data(sourceIndex, AddressListModel::TxNRole).toInt();
      const uint64_t balance = addressModel_->data(sourceIndex, AddressListModel::BalanceRole).toULongLong();
      const auto &walletName = addressModel_->data(sourceIndex, AddressListModel::WalletNameRole).toString();
      const auto &index = addressModel_->data(sourceIndex, AddressListModel::AddressIndexRole).toString().toStdString();
      const auto &comment = addressModel_->data(sourceIndex, AddressListModel::AddressCommentRole).toString().toStdString();
      try {
         const auto &address = bs::Address::fromAddressString(addrStr);
         auto dialog = new AddressDetailDialog(address, logger_, wltType, balance
            , txn, walletName, index, comment, this);
         addrDetDialogs_[ledgerFilter] = dialog;
         connect(dialog, &AddressDetailDialog::needTXDetails, this, &WalletsWidget::needTXDetails);
         connect(dialog, &QDialog::finished, [dialog, ledgerFilter, this](int) {
            dialog->deleteLater();
            addrDetDialogs_.erase(ledgerFilter);
         });
         dialog->show();
      }
      catch (const std::exception &) {}
   }
}

void WalletsWidget::onAddressContextMenu(const QPoint &p)
{
   const auto index = addressSortFilterModel_->mapToSource(ui_->treeViewAddresses->indexAt(p));
   const auto addressIndex = addressModel_->index(index.row(), static_cast<int>(AddressListModel::ColumnAddress));
   try {
      curAddress_ = bs::Address::fromAddressString(addressModel_->data(addressIndex, AddressListModel::Role::AddressRole).toString().toStdString());
   }
   catch (const std::exception &) {
      curAddress_.clear();
      return;
   }
   if (walletsManager_) {
      curWallet_ = walletsManager_->getWalletByAddress(curAddress_);

      if (!curWallet_) {
         logger_->warn("Failed to find wallet for address {}", curAddress_.display());
         return;
      }
      auto contextMenu = new QMenu(this);

      if ((curWallet_->type() == bs::core::wallet::Type::Bitcoin) || (getSelectedWallets().size() == 1)) {
         contextMenu->addAction(actCopyAddr_);
      }
      contextMenu->addAction(actEditComment_);

      const auto &cbAddrBalance = [this, p, contextMenu](std::vector<uint64_t> balances)
      {
         if (/*(curWallet_ == walletsManager_->getSettlementWallet()) &&*/ walletsManager_->getAuthWallet()
            /*&& (curWallet_->getAddrTxN(curAddress_) == 1)*/ && balances[0]) {
            contextMenu->addAction(actRevokeSettl_);
         }
         emit showContextMenu(contextMenu, ui_->treeViewAddresses->mapToGlobal(p));
      };

      auto balanceVec = curWallet_->getAddrBalance(curAddress_);
      if (balanceVec.size() == 0)
         emit showContextMenu(contextMenu, ui_->treeViewAddresses->mapToGlobal(p));
      else
         cbAddrBalance(balanceVec);
      return;
   }

   curWalletId_ = addressModel_->data(addressIndex, AddressListModel::Role::WalletIdRole).toString().toStdString();
   if (curWalletId_.empty()) {
      logger_->warn("Failed to find wallet for address {}", curAddress_.display());
      return;
   }
   curComment_ = addressModel_->data(addressIndex, AddressListModel::Role::AddressCommentRole).toString().toStdString();
   auto contextMenu = new QMenu(this);

   if ((static_cast<bs::core::wallet::Type>(addressModel_->data(addressIndex, AddressListModel::Role::WalletTypeRole).toInt())
      == bs::core::wallet::Type::Bitcoin) || (getSelectedWallets().size() == 1)) {
      contextMenu->addAction(actCopyAddr_);
   }
   contextMenu->addAction(actEditComment_);

   const auto &cbAddrBalance = [this, p, contextMenu](std::vector<uint64_t> balances)
   {
      if (/*walletsManager_->getAuthWallet() && balances[0]*/false) {   //FIXME
         contextMenu->addAction(actRevokeSettl_);
      }
      emit showContextMenu(contextMenu, ui_->treeViewAddresses->mapToGlobal(p));
   };

   //TODO: get address balance and add revoke action if needed
   contextMenu ->exec(ui_->treeViewAddresses->mapToGlobal(p));
}

void WalletsWidget::onShowContextMenu(QMenu *menu, QPoint where)
{
   menu->exec(where);
}

   // Not used
//void WalletsWidget::onWalletContextMenu(const QPoint &p)
//{
//   const auto node = walletsModel_->getNode(ui_->treeViewWallets->indexAt(p));
//   if (!node || node->hasChildren() || !node->parent() || (node->parent()->type() != WalletNode::Type::Root)) {
//      return;
//   }

//   QMenu contextMenu;
//   actDeleteWallet_->setData(QString::fromStdString(node->id()));
//   contextMenu.addAction(actDeleteWallet_);

//   contextMenu.exec(ui_->treeViewWallets->mapToGlobal(p));
//}

void WalletsWidget::updateAddresses()
{
   const auto &selectedWallets = getSelectedWallets();

   if (ui_->treeViewWallets->selectionModel()->hasSelection()) {
      prevSelectedWalletRow_ = ui_->treeViewWallets->selectionModel()->selectedIndexes().first().row();
   }

   if (selectedWallets.size() == 1) {
      curWalletId_ = *selectedWallets.at(0).ids.cbegin();
   }
   else {
      curWalletId_.clear();
   }
   prevSelectedWallets_ = selectedWallets;

   if (!applyPreviousSelection()) {
      addressModel_->setWallets(selectedWallets, true, filterBtcOnly());
   }
}

bool WalletsWidget::applyPreviousSelection()
{
   // keep wallet row selection (highlighting)
   bool isWalletSelectionChanged = (prevSelectedWalletRow_ == -1);
   if (!ui_->treeViewWallets->selectionModel()->hasSelection() && prevSelectedWalletRow_ != -1) {
      auto index = ui_->treeViewWallets->model()->index(prevSelectedWalletRow_, 0);
      if (index.isValid()) {
         isWalletSelectionChanged = true;
         ui_->treeViewWallets->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
      }
   }
   ui_->treeViewWallets->horizontalScrollBar()->setValue(walletsScrollPos_.x());
   ui_->treeViewWallets->verticalScrollBar()->setValue(walletsScrollPos_.y());


   // keep address row selection (highlighting)
   if (!ui_->treeViewAddresses->selectionModel()->hasSelection() && prevSelectedAddressRow_ != -1) {
      auto index = ui_->treeViewAddresses->model()->index(prevSelectedAddressRow_, 0);
      if (index.isValid()) {
         ui_->treeViewAddresses->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
      }
   }
   ui_->treeViewAddresses->horizontalScrollBar()->setValue(addressesScrollPos_.x());
   ui_->treeViewAddresses->verticalScrollBar()->setValue(addressesScrollPos_.y());

   return isWalletSelectionChanged;
}

bool WalletsWidget::filterBtcOnly() const
{
   auto selectedNode = getSelectedNode();
   return selectedNode ? (selectedNode->type() == WalletNode::Type::WalletPrimary
      || selectedNode->type() == WalletNode::Type::WalletRegular) : false;
}

void WalletsWidget::treeViewAddressesSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
   if (ui_->treeViewAddresses->selectionModel()->hasSelection()) {
      prevSelectedAddressRow_ = ui_->treeViewAddresses->selectionModel()->selectedIndexes().first().row();
   }
}

void WalletsWidget::treeViewAddressesLayoutChanged()
{
   // keep address row selection (highlighting)
   if (!ui_->treeViewAddresses->selectionModel()->hasSelection() && prevSelectedAddressRow_ != -1) {
      auto index = ui_->treeViewAddresses->model()->index(prevSelectedAddressRow_, 0);
      if (index.isValid())
         ui_->treeViewAddresses->selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
   }
}

void WalletsWidget::scrollChanged()
{
   if (ui_->treeViewWallets->model()->rowCount() > 0) {
      walletsScrollPos_.setX(ui_->treeViewWallets->horizontalScrollBar()->value());
      walletsScrollPos_.setY(ui_->treeViewWallets->verticalScrollBar()->value());
   }

   if (ui_->treeViewAddresses->model()->rowCount() > 0) {
      addressesScrollPos_.setX(ui_->treeViewAddresses->horizontalScrollBar()->value());
      addressesScrollPos_.setY(ui_->treeViewAddresses->verticalScrollBar()->value());
   }
}

void WalletsWidget::onWalletsSynchronized()
{
   if (walletsManager_->hasPrimaryWallet()) {
      int i = 0;
      for (const auto &hdWallet : walletsManager_->hdWallets()) {
         if (hdWallet->isPrimary()) {
            ui_->treeViewWallets->selectionModel()->select(walletsModel_->index(i, 0)
               , QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            break;
         }
         i++;
      }
   }
   else if (!walletsManager_->hdWallets().empty()){
      ui_->treeViewWallets->selectionModel()->select(walletsModel_->index(0, 0), QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
   }
}

void WalletsWidget::onWalletBalanceChanged(std::string walletId)
{
   const auto &selectedWallets = getSelectedWallets();
   bool changedSelected = false;
   for (const auto &wallet : selectedWallets) {
      if (*wallet.ids.cbegin() == walletId) {
         changedSelected = true;
         break;
      }
   }
   if (changedSelected) {
      addressModel_->setWallets(selectedWallets, false, filterBtcOnly());
   }
}

void WalletsWidget::onNewWallet()
{
   emit newWalletCreationRequest();
   if (signingContainer_) {
      if (!signingContainer_->isOffline()) {
         NewWalletDialog newWalletDialog(false, appSettings_, this);

         switch (newWalletDialog.exec()) {
         case NewWalletDialog::CreateNew:
            CreateNewWallet();
            break;
         case NewWalletDialog::ImportExisting:
            ImportNewWallet();
            break;
         case NewWalletDialog::ImportHw:
            ImportHwWallet();
            break;
         case NewWalletDialog::Cancel:
            break;
         }
      } else {
         ImportNewWallet();
      }
   }
   else {
      NewWalletDialog newWalletDialog(false, this);
      switch (newWalletDialog.exec()) {
      case NewWalletDialog::CreateNew:
         CreateNewWallet();
         break;
      case NewWalletDialog::ImportExisting:
         ImportNewWallet();
         break;
      case NewWalletDialog::ImportHw:
         ImportHwWallet();
         break;
      case NewWalletDialog::Cancel:
         break;
      default:
         showError(tr("Unknown new wallet choice"));
         break;
      }
   }
}

void WalletsWidget::CreateNewWallet()
{
   if (signingContainer_) {
      signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::CreateWallet);
   }
   else {
      emit needWalletDialog(bs::signer::ui::GeneralDialogType::CreateWallet);
   }
}

void WalletsWidget::ImportNewWallet()
{
   if (signingContainer_) {
      signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ImportWallet);
   }
   else {
      emit needWalletDialog(bs::signer::ui::GeneralDialogType::ImportWallet);
   }
}

void WalletsWidget::ImportHwWallet()
{
   if (signingContainer_) {
      signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ImportHwWallet);
   }
   else {
      emit needWalletDialog(bs::signer::ui::GeneralDialogType::ImportHwWallet);
   }
}

void WalletsWidget::shortcutActivated(ShortcutType s)
{
   switch (s) {
      case ShortcutType::Alt_1 : {
         ui_->treeViewWallets->activate();
      }
         break;

      case ShortcutType::Alt_2 : {
         ui_->treeViewAddresses->activate();
      }
         break;

      default :
         break;
   }
}

void WalletsWidget::onFilterSettingsChanged()
{
   auto filterSettings = getUIFilterSettings();

   appSettings_->set(ApplicationSettings::WalletFiltering, filterSettings);

   updateAddressFilters(filterSettings);
}

void WalletsWidget::onEnterKeyInAddressesPressed(const QModelIndex &index)
{
   showAddressProperties(index);
}

void WalletsWidget::onEnterKeyInWalletsPressed(const QModelIndex &index)
{
   showWalletProperties(index);
}

int WalletsWidget::getUIFilterSettings() const
{
   AddressSortFilterModel::Filter filter;

   if (ui_->pushButtonEmpty->isChecked()) {
      filter |= AddressSortFilterModel::HideEmpty;
   }
   if (ui_->pushButtonInternal->isChecked()) {
      filter |= AddressSortFilterModel::HideInternal;
   }
   if (ui_->pushButtonExternal->isChecked()) {
      filter |= AddressSortFilterModel::HideExternal;
   }
   if (ui_->pushButtonUsed->isChecked()) {
      filter |= AddressSortFilterModel::HideUsedEmpty;
   }

   return static_cast<int>(filter);
}

void WalletsWidget::updateAddressFilters(int filterSettings)
{
   addressSortFilterModel_->setFilter(static_cast<AddressSortFilterModel::Filter>(filterSettings));
}

void WalletsWidget::showInfo(bool report, const QString &title, const QString &text) const
{
   if (!report) {
      return;
   }
   BSMessageBox(BSMessageBox::success, title, text).exec();
}

void WalletsWidget::showError(const QString &text) const
{
   BSMessageBox(BSMessageBox::critical, tr("Wallets managing error"), text).exec();
}

void WalletsWidget::onCopyAddress()
{
   qApp->clipboard()->setText(QString::fromStdString(curAddress_.display()));
}

void WalletsWidget::onEditAddrComment()
{
   if ((!curWallet_ && curWalletId_.empty()) || curAddress_.empty()) {
      return;
   }
   bool isOk = false;
   std::string oldComment;
   if (curWallet_) {
      oldComment = curWallet_->getAddressComment(curAddress_);
   }
   else {
      oldComment = curComment_;
   }
   const auto comment = QInputDialog::getText(this, tr("Edit Comment")
      , tr("Enter new comment for address %1:").arg(QString::fromStdString(curAddress_.display()))
      , QLineEdit::Normal, QString::fromStdString(oldComment), &isOk);
   if (isOk) {
      if (curWallet_) {
         if (!curWallet_->setAddressComment(curAddress_, comment.toStdString())) {
            BSMessageBox(BSMessageBox::critical, tr("Address Comment"), tr("Failed to save comment")).exec();
         }
      }
      else {
         emit setAddrComment(curWalletId_, curAddress_, comment.toStdString());
      }
   }
}

void WalletsWidget::onRevokeSettlement()
{
   BSMessageBox(BSMessageBox::info, tr("Settlement Revoke"), tr("Doesn't work currently"), this).exec();
}

void WalletsWidget::onTXSigned(unsigned int id, BinaryData signedTX, bs::error::ErrorCode result)
{
   if (!revokeReqId_ || (revokeReqId_ != id)) {
      return;
   }
   revokeReqId_ = 0;
   const auto &title = tr("Settlement Revoke");
   if (result != bs::error::ErrorCode::NoError) {
      BSMessageBox(BSMessageBox::critical, title, tr("Failed to sign revoke pay-out"), bs::error::ErrorCodeToString(result)).exec();
      return;
   }

   if (!armory_->broadcastZC(signedTX).empty()) {
//      walletsManager_->getSettlementWallet()->setTransactionComment(signedTX, "Settlement Revoke"); //TODO later
   }
   else {
      BSMessageBox(BSMessageBox::critical, title, tr("Failed to send transaction to mempool")).exec();
   }
}

   // Not used
//void WalletsWidget::onDeleteWallet()
//{
//   const auto action = qobject_cast<QAction *>(sender());
//   const auto walletId = action ? action->data().toString() : QString();
//   if (walletId.isEmpty()) {
//      BSMessageBox(BSMessageBox::critical, tr("Wallet Delete"), tr("Failed to delete wallet"), this).exec();
//      return;
//   }
//}
