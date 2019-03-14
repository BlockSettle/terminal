
#include "WalletsWidget.h"
#include "ui_WalletsWidget.h"

#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMenu>
#include <QModelIndex>
#include <QSortFilterProxyModel>

#include "AddressDetailDialog.h"
#include "AddressListModel.h"
#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "BSMessageBox.h"
#include "NewWalletDialog.h"
#include "NewWalletSeedDialog.h"
#include "SelectAddressDialog.h"
#include "SignContainer.h"
#include "WalletImporter.h"
#include "WalletsViewModel.h"
#include "WalletWarningDialog.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncSettlementWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "TreeViewWithEnterKey.h"
#include "NewWalletSeedConfirmDialog.h"
#include "ManageEncryption/VerifyWalletBackupDialog.h"
#include "ManageEncryption/WalletDeleteDialog.h"
#include "ManageEncryption/CreateWalletDialog.h"
#include "ManageEncryption/CreateWalletDialog.h"
#include "ManageEncryption/ImportWalletDialog.h"
#include "ManageEncryption/ImportWalletTypeDialog.h"
#include "ManageEncryption/WalletBackupDialog.h"
#include "ManageEncryption/RootWalletPropertiesDialog.h"

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

   actDeleteWallet_ = new QAction(tr("&Delete Permanently"));
   connect(actDeleteWallet_, &QAction::triggered, this, &WalletsWidget::onDeleteWallet);

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

   connect(signingContainer_.get(), &SignContainer::TXSigned, this, &WalletsWidget::onTXSigned);

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
}

void WalletsWidget::setUsername(const QString& username)
{
   username_ = username;
}

void WalletsWidget::InitWalletsView(const std::string& defaultWalletId)
{
   walletsModel_ = new WalletsViewModel(walletsManager_, defaultWalletId, signingContainer_, ui_->treeViewWallets);
   ui_->treeViewWallets->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewWallets->setModel(walletsModel_);
   ui_->treeViewWallets->setFocus(Qt::ActiveWindowFocusReason);
   ui_->treeViewWallets->setItemsExpandable(true);
   ui_->treeViewWallets->setRootIsDecorated(true);
   ui_->treeViewWallets->setExpandsOnDoubleClick(false);
   // show the column as per BST-1520
   //ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnID));
   walletsModel_->LoadWallets();

   connect(ui_->walletPropertiesButton, &QPushButton::clicked, this, &WalletsWidget::showSelectedWalletProperties);
   connect(ui_->createWalletButton, &QPushButton::clicked, this, &WalletsWidget::onNewWallet);
   connect(ui_->treeViewWallets, &QTreeView::doubleClicked, this, &WalletsWidget::showWalletProperties);
   connect(ui_->treeViewAddresses, &QTreeView::doubleClicked, this, &WalletsWidget::showAddressProperties);

   ui_->treeViewAddresses->setContextMenuPolicy(Qt::CustomContextMenu);
   ui_->treeViewWallets->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui_->treeViewAddresses, &QTreeView::customContextMenuRequested, this, &WalletsWidget::onAddressContextMenu);
   connect(ui_->treeViewWallets, &QTreeView::customContextMenuRequested, this, &WalletsWidget::onWalletContextMenu);

   addressModel_ = new AddressListModel(walletsManager_, this);
   addressSortFilterModel_ = new AddressSortFilterModel(this);
   addressSortFilterModel_->setSourceModel(addressModel_);
   addressSortFilterModel_->setSortRole(AddressListModel::SortRole);

   ui_->treeViewAddresses->setUniformRowHeights(true);
   ui_->treeViewAddresses->setModel(addressSortFilterModel_);
   ui_->treeViewAddresses->sortByColumn(2, Qt::DescendingOrder);
   ui_->treeViewAddresses->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->treeViewAddresses->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

   updateAddresses();
   connect(ui_->treeViewWallets->selectionModel(), &QItemSelectionModel::selectionChanged, this, &WalletsWidget::updateAddresses);
   connect(walletsModel_, &WalletsViewModel::updateAddresses, this, &WalletsWidget::updateAddresses);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletBalanceChanged, this, &WalletsWidget::onWalletBalanceChanged, Qt::QueuedConnection);
}

std::vector<std::shared_ptr<bs::sync::Wallet>> WalletsWidget::getSelectedWallets() const
{
   auto indexes = ui_->treeViewWallets->selectionModel()->selectedIndexes();
   if (!indexes.isEmpty()) {
      return walletsModel_->getWallets(indexes.first());
   }
   return {};
}

std::vector<std::shared_ptr<bs::sync::Wallet>> WalletsWidget::getFirstWallets() const
{
   if (walletsModel_->rowCount()) {
      return walletsModel_->getWallets(walletsModel_->index(0, 0));
   } else {
      return {};
   }
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
   if (hdWallet != nullptr) {
//      RootWalletPropertiesDialog(logger_, hdWallet, walletsManager_, armory_, signingContainer_
//         , walletsModel_, appSettings_, assetManager_, this).exec();

      RootWalletPropertiesDialog(logger_, hdWallet, walletsManager_, armory_, signingContainer_
         , walletsModel_, appSettings_, connectionManager_, assetManager_, this).exec();
   }
}

void WalletsWidget::showAddressProperties(const QModelIndex& index)
{
   auto sourceIndex = addressSortFilterModel_->mapToSource(index);
   const auto walletId = addressModel_->data(sourceIndex, AddressListModel::WalletIdRole).toString().toStdString();
   const auto wallet = walletsManager_->getWalletById(walletId);
   if (!wallet || (wallet->type() == bs::core::wallet::Type::Authentication)) {
      return;
   }

   const auto &addresses = wallet->getUsedAddressList();
   const size_t addrIndex = addressModel_->data(sourceIndex, AddressListModel::AddrIndexRole).toUInt();
   const auto address = (addrIndex < addresses.size()) ? addresses[addrIndex] : bs::Address();

   AddressDetailDialog* dialog = new AddressDetailDialog(address, wallet,
                                                         walletsManager_,
                                                         armory_, logger_,
                                                         this);
   dialog->exec();
}

void WalletsWidget::onAddressContextMenu(const QPoint &p)
{
   const auto index = addressSortFilterModel_->mapToSource(ui_->treeViewAddresses->indexAt(p));
   const auto addressIndex = addressModel_->index(index.row(), static_cast<int>(AddressListModel::ColumnAddress));
   try {
      curAddress_ = bs::Address(addressModel_->data(addressIndex, AddressListModel::Role::AddressRole).toString());
   }
   catch (const std::exception &) {
      curAddress_.clear();
      return;
   }
   curWallet_ = walletsManager_->getWalletByAddress(curAddress_);

   if (!curWallet_) {
      logger_->warn("Failed to find wallet for address {}", curAddress_.display<std::string>());
      return;
   }
   auto contextMenu = new QMenu(this);

   if ((curWallet_->type() == bs::core::wallet::Type::Bitcoin) || (getSelectedWallets().size() == 1)) {
      contextMenu->addAction(actCopyAddr_);
   }
   contextMenu->addAction(actEditComment_);

   const auto &cbAddrBalance = [this, p, contextMenu](std::vector<uint64_t> balances) {
      if ((curWallet_ == walletsManager_->getSettlementWallet()) && walletsManager_->getAuthWallet()
         /*&& (curWallet_->getAddrTxN(curAddress_) == 1)*/ && balances[0]) {
         contextMenu->addAction(actRevokeSettl_);
      }
      emit showContextMenu(contextMenu, ui_->treeViewAddresses->mapToGlobal(p));
   };
   if (!curWallet_->getAddrBalance(curAddress_, cbAddrBalance)) {
      emit showContextMenu(contextMenu, ui_->treeViewAddresses->mapToGlobal(p));
   }
}

void WalletsWidget::onShowContextMenu(QMenu *menu, QPoint where)
{
   menu->exec(where);
}

void WalletsWidget::onWalletContextMenu(const QPoint &p)
{
   const auto node = walletsModel_->getNode(ui_->treeViewWallets->indexAt(p));
   if (!node || node->hasChildren() || (node->parent()->type() != WalletNode::Type::Root)) {
      return;
   }

   QMenu contextMenu;
   actDeleteWallet_->setData(QString::fromStdString(node->id()));
   contextMenu.addAction(actDeleteWallet_);

   contextMenu.exec(ui_->treeViewWallets->mapToGlobal(p));
}

void WalletsWidget::updateAddresses()
{
   const auto &selectedWallets = getSelectedWallets();
   if (selectedWallets == prevSelectedWallets_) {
      return;
   }
   addressModel_->setWallets(selectedWallets);
   prevSelectedWallets_ = selectedWallets;
}

void WalletsWidget::onWalletBalanceChanged(std::string walletId)
{
   const auto &selectedWallets = getSelectedWallets();
   bool changedSelected = false;
   for (const auto &wallet : selectedWallets) {
      if (wallet->walletId() == walletId) {
         changedSelected = true;
         break;
      }
   }
   if (changedSelected) {
      addressModel_->setWallets(selectedWallets);
   }
}

void WalletsWidget::onNewWallet()
{
   if (!signingContainer_->isOffline()) {
      NewWalletDialog newWalletDialog(false, appSettings_, this);
      if (newWalletDialog.exec() != QDialog::Accepted ) {
         return;
      }

      if (newWalletDialog.isCreate()) {
         CreateNewWallet();
      } else if (newWalletDialog.isImport()) {
         ImportNewWallet();
      }
   } else {
      ImportNewWallet();
   }
}

bool WalletsWidget::CreateNewWallet(bool report)
{
   NetworkType netType = appSettings_->get<NetworkType>(ApplicationSettings::netType);

   bs::core::wallet::Seed walletSeed(netType, CryptoPRNG::generateRandom(32));

   EasyCoDec::Data easyData = walletSeed.toEasyCodeChecksum();

   const auto walletId = walletSeed.getWalletId();

   NewWalletSeedDialog newWalletSeedDialog(QString::fromStdString(walletId)
      , QString::fromStdString(easyData.part1), QString::fromStdString(easyData.part2), this);

   int result = newWalletSeedDialog.exec();
   if (!result) {
      return false;
   }
   // get the user to confirm the seed
   NewWalletSeedConfirmDialog dlg(walletId, netType
      , QString::fromStdString(easyData.part1), QString::fromStdString(easyData.part2), this);
   result = dlg.exec();
   if (!result) {
      return false;
   }
   std::shared_ptr<bs::sync::hd::Wallet> newWallet;
//   CreateWalletDialog createWalletDialog(walletsManager_, signingContainer_
//      , appSettings_->GetHomeDir(), walletSeed, walletId, username_, appSettings_, this);
   CreateWalletDialog createWalletDialog(walletsManager_, signingContainer_
      , walletSeed, walletId, username_, appSettings_, connectionManager_, logger_, this);
   if (createWalletDialog.exec() == QDialog::Accepted) {
      if (createWalletDialog.walletCreated()) {
         newWallet = walletsManager_->getHDWalletById(walletId);
         if (!newWallet) {
            showError(tr("Failed to find newly created wallet"));
            return false;
         }

         if (report) {
            BSMessageBox(BSMessageBox::success
               , tr("%1Wallet Created").arg(createWalletDialog.isNewWalletPrimary() ? tr("Primary ") : QString())
               , tr("Wallet \"%1\" Successfully Created").arg(QString::fromStdString(newWallet->name())), this).exec();
         }

         return true;
      } else {
         showError(tr("Failed to create wallet"));
         return false;
      }
   } else {
      return false;
   }
}

bool WalletsWidget::ImportNewWallet(bool report)
{
   bool disablePrimaryImport = false;

   if (!walletsManager_->hasPrimaryWallet() && assetManager_->privateShares(true).empty()) {
      BSMessageBox q(BSMessageBox::warning, tr("Private Market Import"), tr("Private Market data is missing")
         , tr("You do not have Private Market data available in the BlockSettle Terminal. You must first log "
            "into the BlockSettle trading network from the main menu. A successful login will cause the proper data to be "
            "automatically downloaded. Without this data, you will be unable to receive your Private Market "
            "balances. Are you absolutely certain that you wish to proceed an import that doesn't include "
            "Private Market data? (Upon receiving the data, you will have to re-import the wallet in order to "
            "use the data.)"), this);
      q.exec();
      disablePrimaryImport = true;
   }

   // if signer is not ready - import WO only
   ImportWalletTypeDialog importWalletDialog(signingContainer_->isOffline(), this);

   if (importWalletDialog.exec() == QDialog::Accepted) {
      if (importWalletDialog.type() == ImportWalletTypeDialog::Full) {
         ImportWalletDialog createImportedWallet(walletsManager_
                                                    , signingContainer_
                                                    , assetManager_
                                                    , authMgr_, armory_
                                                    , importWalletDialog.GetSeedData()
                                                    , importWalletDialog.GetChainCodeData()
                                                    , appSettings_
                                                    , connectionManager_
                                                    , logger_
                                                    , username_
                                                    , importWalletDialog.GetName()
                                                    , importWalletDialog.GetDescription()
                                                    , disablePrimaryImport
                                                    , this);

         if (createImportedWallet.exec() == QDialog::Accepted) {
            const auto &importer = createImportedWallet.getWalletImporter();

            const auto &walletId = createImportedWallet.getWalletId();
            walletImporters_[walletId] = importer;

            if (report) {
               BSMessageBox(BSMessageBox::success
                  , tr("%1Wallet Imported").arg(createImportedWallet.ImportedAsPrimary() ? tr("Primary ") : QString())
                  , tr("Wallet \"%1\" Successfully Imported").arg(createImportedWallet.getNewWalletName()), this).exec();
            }
         }
      }
      else {   //TODO: decide on WO wallet import later
      }
   }

   return true;
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
   qApp->clipboard()->setText(curAddress_.display());
}

void WalletsWidget::onEditAddrComment()
{
   if (!curWallet_ || curAddress_.isNull()) {
      return;
   }
   bool isOk = false;
   const auto comment = QInputDialog::getText(this, tr("Edit Comment")
      , tr("Enter new comment for address %1:").arg(curAddress_.display())
      , QLineEdit::Normal, QString::fromStdString(curWallet_->getAddressComment(curAddress_)), &isOk);
   if (isOk) {
      if (!curWallet_->setAddressComment(curAddress_, comment.toStdString())) {
         BSMessageBox(BSMessageBox::critical, tr("Address Comment"), tr("Failed to save comment")).exec();
      }
   }
}

void WalletsWidget::onRevokeSettlement()
{
   const auto &title = tr("Settlement Revoke");
   const auto &settlWallet = walletsManager_->getSettlementWallet();
   const auto &addrIndex = QString::fromStdString(settlWallet->getAddressIndex(curAddress_));
   const auto settlId = BinaryData::CreateFromHex(addrIndex.section(QLatin1Char('.'), 0, 0).toStdString());
   const auto sellAuthKey = BinaryData::CreateFromHex(addrIndex.section(QLatin1Char('.'), 2, 2).toStdString());
   if (addrIndex.isEmpty() || settlId.isNull() || sellAuthKey.isNull()) {
      BSMessageBox(BSMessageBox::critical, title, tr("Unknown settlement address")).exec();
      return;
   }
   const auto ae = settlWallet->getExistingAddress(settlId);
   if (ae.isNull()) {
      BSMessageBox(BSMessageBox::critical, title, tr("Invalid settlement address")).exec();
      return;
   }

   const auto &cbSettlInput = [this, settlWallet, sellAuthKey, title, ae] (UTXO utxo) {
      SelectAddressDialog selectAddressDialog{ walletsManager_, walletsManager_->getDefaultWallet(), this };
      bs::Address recvAddr;
      if (selectAddressDialog.exec() == QDialog::Accepted) {
         recvAddr = selectAddressDialog.getSelectedAddress();
      }
      else {
         return;
      }

      const auto &cbFee = [this, settlWallet, utxo, recvAddr, sellAuthKey, title, ae](float feePerByte) {
         try {
            const auto txReq = settlWallet->createPayoutTXRequest(utxo, recvAddr, feePerByte);
            const auto authAddr = bs::Address::fromPubKey(sellAuthKey, AddressEntryType_P2WPKH);
            revokeReqId_ = signingContainer_->signPayoutTXRequest(txReq, authAddr, settlWallet->getAddressIndex(ae));
         }
         catch (const std::exception &e) {
            BSMessageBox(BSMessageBox::critical, title, tr("Failed to sign revoke pay-out"), QLatin1String(e.what())).exec();
         }
      };
      walletsManager_->estimatedFeePerByte(2, cbFee, this);
   };
   settlWallet->getInputFor(ae, cbSettlInput, false);
}

void WalletsWidget::onTXSigned(unsigned int id, BinaryData signedTX,
   std::string error, bool cancelledByUser)
{
   if (!revokeReqId_ || (revokeReqId_ != id)) {
      return;
   }
   revokeReqId_ = 0;
   const auto &title = tr("Settlement Revoke");
   if (!error.empty()) {
      BSMessageBox(BSMessageBox::critical, title, tr("Failed to sign revoke pay-out"), QString::fromStdString(error)).exec();
      return;
   }

   if (armory_->broadcastZC(signedTX)) {
      walletsManager_->getSettlementWallet()->setTransactionComment(signedTX, "Settlement Revoke");
   }
   else {
      BSMessageBox(BSMessageBox::critical, title, tr("Failed to send transaction to mempool")).exec();
   }
}

void WalletsWidget::onDeleteWallet()
{
   const auto action = qobject_cast<QAction *>(sender());
   const auto walletId = action ? action->data().toString() : QString();
   if (walletId.isEmpty()) {
      BSMessageBox(BSMessageBox::critical, tr("Wallet Delete"), tr("Failed to delete wallet"), this).exec();
      return;
   }
   const auto &wallet = walletsManager_->getWalletById(walletId.toStdString());
   if (!wallet) {
      BSMessageBox(BSMessageBox::critical, tr("Wallet Delete"), tr("Failed to find wallet with id %1").arg(walletId), this).exec();
      return;
   }
   WalletDeleteDialog(wallet, walletsManager_, signingContainer_, appSettings_, connectionManager_
                      , logger_, this).exec();
}
