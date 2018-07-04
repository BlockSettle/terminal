
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
#include "CreateWalletDialog.h"
#include "HDWallet.h"
#include "ImportWalletDialog.h"
#include "ImportWalletTypeDialog.h"
#include "MessageBoxCritical.h"
#include "MessageBoxQuestion.h"
#include "MessageBoxSuccess.h"
#include "NewWalletDialog.h"
#include "RootWalletPropertiesDialog.h"
#include "SelectAddressDialog.h"
#include "SignContainer.h"
#include "VerifyWalletBackupDialog.h"
#include "WalletBackupDialog.h"
#include "WalletCompleteDialog.h"
#include "WalletDeleteDialog.h"
#include "WalletImporter.h"
#include "WalletsManager.h"
#include "WalletsViewModel.h"
#include "WalletWarningDialog.h"
#include "TreeViewWithEnterKey.h"


class AddressSortFilterModel : public QSortFilterProxyModel
{
public:
   AddressSortFilterModel(QObject* parent) : QSortFilterProxyModel(parent) {}

   enum FilterOption
   {
      NoFilter = 0x00,
      HideEmpty = 0x01,
      HideUnused = 0x02,
      HideUsedEmpty = 0x04
   };
   Q_DECLARE_FLAGS(Filter, FilterOption)

   bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override
   {
      if (filterMode_ & HideUnused) {
         int txCount = sourceModel()->data(sourceModel()->index(source_row, AddressListModel::ColumnTxCount, source_parent)).toInt();
         if (txCount == 0) {
            return false;
         }
      }

      if (filterMode_ & HideEmpty) {
         double balance = QLocale().toDouble(sourceModel()->data(sourceModel()->index(source_row, AddressListModel::ColumnBalance, source_parent)).toString());
         if (qFuzzyIsNull(balance)) {
            return false;
         }
      }

      if (filterMode_ & HideUsedEmpty) {
         int txCount = sourceModel()->data(sourceModel()->index(source_row, AddressListModel::ColumnTxCount, source_parent)).toInt();
         if (txCount != 0) {
            double balance = QLocale().toDouble(sourceModel()->data(sourceModel()->index(source_row, AddressListModel::ColumnBalance, source_parent)).toString());
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
         QVariant leftData = sourceModel()->data(left);
         QVariant rightData = sourceModel()->data(right);

         if (leftData != rightData) {
            if (leftData.type() == QVariant::String && rightData.type() == QVariant::String) {
               bool leftConverted = false;
               double leftDoubleValue = leftData.toString().toDouble(&leftConverted);

               bool rightConverted = false;
               double rightDoubleValue = rightData.toString().toDouble(&rightConverted);

               if (leftConverted && rightConverted) {
                  return leftDoubleValue < rightDoubleValue;
               }
            }
         } else {
            const QModelIndex lTxnIndex = sourceModel()->index(left.row(), AddressListModel::ColumnTxCount);
            const QModelIndex rTxnIndex = sourceModel()->index(right.row(), AddressListModel::ColumnTxCount);

            return (sourceModel()->data(lTxnIndex) < sourceModel()->data(rTxnIndex));
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
   , ui(new Ui::WalletsWidget())
   , walletsManager_(nullptr)
   , walletsModel_(nullptr)
   , addressModel_(nullptr)
   , addressSortFilterModel_(nullptr)
{
   ui->setupUi(this);

   ui->gridLayout->setRowStretch(0, 1);
   ui->gridLayout->setRowStretch(1, 2);

   actCopyAddr_ = new QAction(tr("&Copy Address"), this);
   connect(actCopyAddr_, &QAction::triggered, this, &WalletsWidget::onCopyAddress);

   actEditComment_ = new QAction(tr("&Edit Comment"));
   connect(actEditComment_, &QAction::triggered, this, &WalletsWidget::onEditAddrComment);

   actRevokeSettl_ = new QAction(tr("&Revoke Settlement"));
   connect(actRevokeSettl_, &QAction::triggered, this, &WalletsWidget::onRevokeSettlement);

   actDeleteWallet_ = new QAction(tr("&Delete Permanently"));
   connect(actDeleteWallet_, &QAction::triggered, this, &WalletsWidget::onDeleteWallet);

   connect(ui->treeViewAddresses, &TreeViewWithEnterKey::enterKeyPressed,
           this, &WalletsWidget::onEnterKeyInAddressesPressed);
   connect(ui->treeViewWallets, &WalletsTreeView::enterKeyPressed,
           this, &WalletsWidget::onEnterKeyInWalletsPressed);
}

void WalletsWidget::init(const std::shared_ptr<WalletsManager> &manager, const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ApplicationSettings> &applicationSettings, const std::shared_ptr<AssetManager> &assetMgr
   , const std::shared_ptr<AuthAddressManager> &authMgr)
{
   walletsManager_ = manager;
   signingContainer_ = container;
   appSettings_ = applicationSettings;
   assetManager_ = assetMgr;
   authMgr_ = authMgr;

   connect(signingContainer_.get(), &SignContainer::TXSigned, this, &WalletsWidget::onTXSigned);

   const auto &defWallet = walletsManager_->GetDefaultWallet();
   InitWalletsView(defWallet ? defWallet->GetWalletId() : std::string{});

   auto filter = appSettings_->get<int>(ApplicationSettings::WalletFiltering);

   ui->pushButtonEmpty->setChecked(filter & AddressSortFilterModel::HideEmpty);
   ui->pushButtonUnused->setChecked(filter & AddressSortFilterModel::HideUnused);
   ui->pushButtonUsed->setChecked(filter & AddressSortFilterModel::HideUsedEmpty);

   updateAddressFilters(filter);

   for (auto button : {ui->pushButtonEmpty, ui->pushButtonUnused, ui->pushButtonUsed}) {
      connect(button, &QPushButton::toggled, this, &WalletsWidget::onFilterSettingsChanged);
   }
}

void WalletsWidget::InitWalletsView(const std::string& defaultWalletId)
{
   walletsModel_ = new WalletsViewModel(walletsManager_, defaultWalletId, signingContainer_, ui->treeViewWallets);
   ui->treeViewWallets->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui->treeViewWallets->setModel(walletsModel_);
   ui->treeViewWallets->setFocus(Qt::ActiveWindowFocusReason);
   ui->treeViewWallets->setItemsExpandable(true);
   ui->treeViewWallets->setRootIsDecorated(true);
   ui->treeViewWallets->setExpandsOnDoubleClick(false);
   walletsModel_->LoadWallets();

   connect(ui->treeViewWallets->selectionModel(), &QItemSelectionModel::selectionChanged, this, &WalletsWidget::updateAddresses);

   connect(ui->walletPropertiesButton, &QPushButton::clicked, this, &WalletsWidget::showSelectedWalletProperties);
   connect(ui->createWalletButton, &QPushButton::clicked, this, &WalletsWidget::onNewWallet);
   connect(ui->treeViewWallets, &QTreeView::doubleClicked, this, &WalletsWidget::showWalletProperties);
   connect(ui->treeViewAddresses, &QTreeView::doubleClicked, this, &WalletsWidget::showAddressProperties);

   ui->treeViewAddresses->setContextMenuPolicy(Qt::CustomContextMenu);
   ui->treeViewWallets->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui->treeViewAddresses, &QTreeView::customContextMenuRequested, this, &WalletsWidget::onAddressContextMenu);
   connect(ui->treeViewWallets, &QTreeView::customContextMenuRequested, this, &WalletsWidget::onWalletContextMenu);

   addressModel_ = new AddressListModel(walletsManager_, this);
   addressSortFilterModel_ = new AddressSortFilterModel(this);
   addressSortFilterModel_->setSourceModel(addressModel_);
   addressSortFilterModel_->setSortRole(AddressListModel::SortRole);

   ui->treeViewAddresses->setUniformRowHeights(true);
   ui->treeViewAddresses->setModel(addressSortFilterModel_);
   ui->treeViewAddresses->sortByColumn(2, Qt::DescendingOrder);
   ui->treeViewAddresses->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui->treeViewAddresses->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

   updateAddresses();
}

std::vector<WalletsManager::wallet_gen_type> WalletsWidget::GetSelectedWallets() const
{
   auto indexes = ui->treeViewWallets->selectionModel()->selectedIndexes();
   if (!indexes.isEmpty()) {
      return walletsModel_->getWallets(indexes.first());
   }
   return {};
}

void WalletsWidget::showSelectedWalletProperties()
{
   auto indexes = ui->treeViewWallets->selectionModel()->selectedIndexes();
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
      RootWalletPropertiesDialog(hdWallet, walletsManager_, signingContainer_, walletsModel_, appSettings_
         , assetManager_, this).exec();
      return;
   }
}

void WalletsWidget::showAddressProperties(const QModelIndex& index)
{
   auto sourceIndex = addressSortFilterModel_->mapToSource(index);
   const auto walletId = addressModel_->data(sourceIndex, AddressListModel::WalletIdRole).toString().toStdString();
   const auto wallet = walletsManager_->GetWalletById(walletId);
   if (!wallet || (wallet->GetType() == bs::wallet::Type::Authentication)) {
      return;
   }

   const auto &addresses = wallet->GetUsedAddressList();
   const size_t addrIndex = addressModel_->data(sourceIndex, AddressListModel::AddrIndexRole).toUInt();
   const auto address = (addrIndex < addresses.size()) ? addresses[addrIndex] : bs::Address();

   AddressDetailDialog* dialog = new AddressDetailDialog(address, wallet, walletsManager_, this);
   dialog->exec();
}

void WalletsWidget::onAddressContextMenu(const QPoint &p)
{
   const auto index = addressSortFilterModel_->mapToSource(ui->treeViewAddresses->indexAt(p));
   const auto addressIndex = addressModel_->index(index.row(), static_cast<int>(AddressListModel::ColumnAddress));
   curAddress_ = bs::Address(addressModel_->data(addressIndex).toString());
   curWallet_ = walletsManager_->GetWalletByAddress(curAddress_);
   if (curWallet_) {
      curAddress_ = curWallet_->GetUsedAddressList()[addressIndex.row()];
   }

   QMenu contextMenu;
   contextMenu.addAction(actCopyAddr_);

   if (curWallet_) {
      contextMenu.addAction(actEditComment_);
   }
   if ((curWallet_ == walletsManager_->GetSettlementWallet()) && walletsManager_->GetAuthWallet()
      /*&& (curWallet_->getAddrTxN(curAddress_) == 1)*/ && curWallet_->getAddrBalance(curAddress_)[0]) {
      contextMenu.addAction(actRevokeSettl_);
   }

   contextMenu.exec(ui->treeViewAddresses->mapToGlobal(p));
}


void WalletsWidget::onWalletContextMenu(const QPoint &p)
{
   const auto node = walletsModel_->getNode(ui->treeViewWallets->indexAt(p));
   if (!node || node->hasChildren() || (node->parent()->type() != WalletNode::Type::Root)) {
      return;
   }

   QMenu contextMenu;
   actDeleteWallet_->setData(QString::fromStdString(node->id()));
   contextMenu.addAction(actDeleteWallet_);

   contextMenu.exec(ui->treeViewWallets->mapToGlobal(p));
}

void WalletsWidget::updateAddresses()
{
   addressModel_->setWallets(GetSelectedWallets());
}

void WalletsWidget::onNewWallet()
{
   NewWalletDialog newWalletDialog(false, this);
   if (newWalletDialog.exec() != QDialog::Accepted ) {
      return;
   }

   if (newWalletDialog.isCreate()) {
      CreateNewWallet(false);
   } else if (newWalletDialog.isImport()) {
      ImportNewWallet(false);
   }
}

bool WalletsWidget::CreateNewWallet(bool primary, bool report)
{
   std::shared_ptr<bs::hd::Wallet> newWallet;
   CreateWalletDialog createWalletDialog(walletsManager_, signingContainer_, primary, this);
   if (createWalletDialog.exec() == QDialog::Accepted) {
      if (createWalletDialog.walletCreated()) {
         newWallet = walletsManager_->GetHDWalletById(createWalletDialog.getNewWalletId());
         if (!newWallet) {
            showError(tr("Failed to find newly created wallet"));
            return false;
         }

         if (report) {
            WalletCreateCompleteDialog completedDialog(QString::fromStdString(newWallet->getName())
               , createWalletDialog.isNewWalletPrimary(), this);
            completedDialog.exec();
         }

         return WalletBackupAndVerify(newWallet, signingContainer_, this);
      } else {
         showError(tr("Failed to create wallet"));
         return false;
      }
   } else {
      return false;
   }
}

bool WalletsWidget::ImportNewWallet(bool primary, bool report)
{
   if (primary && assetManager_->privateShares(true).empty()) {
      MessageBoxQuestion q(tr("Private Market Import"), tr("Private Market data is missing")
         , tr("If you want to import all available Private Market wallets now, you need to abort the process"
            " and login to Celer to receive this data first. Otherwise you can do it later on manual rescan"
            " of the imported wallet. Abort importing now?"), this);
      if (q.exec() == QDialog::Accepted) {
         return false;
      }
   }
   ImportWalletTypeDialog importWalletDialog(this);
   if (importWalletDialog.exec() == QDialog::Accepted) {
      if (importWalletDialog.type() == ImportWalletTypeDialog::Full) {
         ImportWalletDialog createImportedWallet(walletsManager_, signingContainer_
            , assetManager_, authMgr_, importWalletDialog.GetSeedData()
            , importWalletDialog.GetChainCodeData(), appSettings_
            , importWalletDialog.GetName(), importWalletDialog.GetDescription()
            , primary, this);

         if (createImportedWallet.exec() == QDialog::Accepted) {
            const auto &importer = createImportedWallet.getWalletImporter();

            const auto &walletId = createImportedWallet.getWalletId();
            walletImporters_[walletId] = importer;

            if (report) {
               WalletImportCompleteDialog completedDialog(createImportedWallet.getNewWalletName()
                  , createImportedWallet.ImportedAsPrimary(), this);
               completedDialog.exec();
            }
         }
      }
      else {
         const QFileInfo fi(importWalletDialog.GetWatchinOnlyFileName());
         const auto targetFile = QString::fromStdString(walletsManager_->GetWalletsPath() + "/") + fi.fileName();
         const auto title = tr("Wallet import error");
         if (QFile(targetFile).exists()) {
            MessageBoxCritical(title, tr("Watching-only wallet file %1 already exists!").arg(targetFile)).exec();
            return false;
         }
         if (!QFile::copy(importWalletDialog.GetWatchinOnlyFileName(), targetFile)) {
            MessageBoxCritical(title, tr("Failed to copy watching-only wallet file to %1").arg(targetFile)).exec();
            return false;
         }
         const auto &newWallet = std::make_shared<bs::hd::Wallet>(targetFile.toStdString());
         if (!newWallet) {
            MessageBoxCritical(title, tr("Failed to load watching-only wallet from %1").arg(targetFile)).exec();
            return false;
         }
         if (walletsManager_->GetHDWalletById(newWallet->getWalletId()) != nullptr) {
            MessageBoxCritical(title, tr("Watching-only wallet with id %1 already exists!")
               .arg(QString::fromStdString(newWallet->getWalletId()))).exec();
            return false;
         }
         walletsManager_->AddWallet(newWallet);
         if (report) {
            WalletCreateCompleteDialog(QString::fromStdString(newWallet->getName()), false, this).exec();
         }
      }
   }

   return true;
}

void WalletsWidget::shortcutActivated(ShortcutType s)
{
   switch (s) {
      case ShortcutType::Alt_1 : {
         ui->treeViewWallets->activate();
      }
         break;

      case ShortcutType::Alt_2 : {
         ui->treeViewAddresses->activate();
      }
         break;

      default :
         break;
   }
}

void WalletsWidget::onImportComplete(const std::string &walletId)
{
   walletsManager_->onWalletImported(walletId);
   walletImporters_.erase(walletId);
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

   if (ui->pushButtonEmpty->isChecked()) {
      filter |= AddressSortFilterModel::HideEmpty;
   }
   if (ui->pushButtonUnused->isChecked()) {
      filter |= AddressSortFilterModel::HideUnused;
   }
   if (ui->pushButtonUsed->isChecked()) {
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
   MessageBoxSuccess(title, text).exec();
}

void WalletsWidget::showError(const QString &text) const
{
   MessageBoxCritical(tr("Wallets managing error"), text).exec();
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
      , QLineEdit::Normal, QString::fromStdString(curWallet_->GetAddressComment(curAddress_)), &isOk);
   if (isOk) {
      if (!curWallet_->SetAddressComment(curAddress_, comment.toStdString())) {
         MessageBoxCritical(tr("Address Comment"), tr("Failed to save comment")).exec();
      }
   }
}

void WalletsWidget::onRevokeSettlement()
{
   const auto &title = tr("Settlement Revoke");
   const auto &settlWallet = walletsManager_->GetSettlementWallet();
   const auto &addrIndex = QString::fromStdString(settlWallet->GetAddressIndex(curAddress_));
   const auto settlId = BinaryData::CreateFromHex(addrIndex.section(QLatin1Char('.'), 0, 0).toStdString());
   const auto sellAuthKey = BinaryData::CreateFromHex(addrIndex.section(QLatin1Char('.'), 2, 2).toStdString());
   if (addrIndex.isEmpty() || settlId.isNull() || sellAuthKey.isNull()) {
      MessageBoxCritical(title, tr("Unknown settlement address")).exec();
      return;
   }
   const auto &ae = settlWallet->getExistingAddress(settlId);
   if (!ae) {
      MessageBoxCritical(title, tr("Invalid settlement address")).exec();
      return;
   }

   UTXO utxo;
   try {
      utxo = settlWallet->GetInputFor(ae, false);
      if (!utxo.isInitialized()) {
         throw std::runtime_error("missing input");
      }
    }
   catch (const std::exception &e) {
      MessageBoxCritical(title, tr("Failed to find pay-in input"), QLatin1String(e.what())).exec();
      return;
   }

   SelectAddressDialog selectAddressDialog{ walletsManager_, walletsManager_->GetDefaultWallet(), this };
   bs::Address recvAddr;
   if (selectAddressDialog.exec() == QDialog::Accepted) {
      recvAddr = selectAddressDialog.getSelectedAddress();
   }
   else {
      return;
   }

   try {
      const auto feePerByte = walletsManager_->estimatedFeePerByte(2);
      const auto txReq = settlWallet->CreatePayoutTXRequest(utxo, recvAddr, feePerByte);
      const auto authAddr = bs::Address::fromPubKey(sellAuthKey, AddressEntryType_P2WPKH);
      revokeReqId_ = signingContainer_->SignPayoutTXRequest(txReq, authAddr, ae);
   }
   catch (const std::exception &e) {
      MessageBoxCritical(title, tr("Failed to sign revoke pay-out"), QLatin1String(e.what())).exec();
   }
}

void WalletsWidget::onTXSigned(unsigned int id, BinaryData signedTX, std::string error)
{
   if (!revokeReqId_ || (revokeReqId_ != id)) {
      return;
   }
   revokeReqId_ = 0;
   const auto &title = tr("Settlement Revoke");
   if (!error.empty()) {
      MessageBoxCritical(title, tr("Failed to sign revoke pay-out"), QString::fromStdString(error)).exec();
      return;
   }

   const auto &bdm = PyBlockDataManager::instance();
   if (!bdm) {
      MessageBoxCritical(title, tr("Unable to send transaction - Armory connection is missing")).exec();
      return;
   }
   if (bdm->broadcastZC(signedTX)) {
      walletsManager_->GetSettlementWallet()->SetTransactionComment(signedTX, "Settlement Revoke");
   }
   else {
      MessageBoxCritical(title, tr("Failed to send transaction to mempool")).exec();
   }
}

void WalletsWidget::onDeleteWallet()
{
   const auto action = qobject_cast<QAction *>(sender());
   const auto walletId = action ? action->data().toString() : QString();
   if (walletId.isEmpty()) {
      MessageBoxCritical(tr("Wallet Delete"), tr("Failed to delete wallet"), this).exec();
      return;
   }
   const auto &wallet = walletsManager_->GetWalletById(walletId.toStdString());
   if (!wallet) {
      MessageBoxCritical(tr("Wallet Delete"), tr("Failed to find wallet with id %1").arg(walletId), this).exec();
      return;
   }
   WalletDeleteDialog(wallet, walletsManager_, signingContainer_, this).exec();
}


bool WalletBackupAndVerify(const std::shared_ptr<bs::hd::Wallet> &wallet, const std::shared_ptr<SignContainer> &container
   , QWidget *parent)
{
   if (!wallet) {
      return false;
   }
   WalletBackupDialog walletBackupDialog(wallet, container, parent);
   if (walletBackupDialog.exec() == QDialog::Accepted) {
      MessageBoxSuccess(QObject::tr("Backup"), QObject::tr("%1 Backup successfully created")
         .arg(walletBackupDialog.isDigitalBackup() ? QObject::tr("Digital") : QObject::tr("Paper"))
            , walletBackupDialog.filePath(), parent).exec();
      if (!walletBackupDialog.isDigitalBackup()) {
         VerifyWalletBackupDialog(wallet, parent).exec();
      }
      WalletWarningDialog(parent).exec();
      return true;
   }

   return false;
}
