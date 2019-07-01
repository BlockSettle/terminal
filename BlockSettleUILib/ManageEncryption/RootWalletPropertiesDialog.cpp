#include "RootWalletPropertiesDialog.h"
#include "ui_WalletPropertiesDialog.h"

#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QStandardPaths>
#include <QSortFilterProxyModel>

#include <bech32/ref/c++/segwit_addr.h>

#include "Address.h"
#include "ApplicationSettings.h"
#include "AssetManager.h"
#include "BSMessageBox.h"
#include "SignContainer.h"
#include "UiUtils.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "WalletsViewModel.h"
#include "WalletsWidget.h"


class CurrentWalletFilter : public QSortFilterProxyModel
{
public:
   CurrentWalletFilter(const std::shared_ptr<bs::sync::hd::Wallet> &wallet, QObject* parent)
      : QSortFilterProxyModel(parent)
      , wallet_(wallet)
   {
   }

   bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override
   {
      if (sourceParent.isValid()) {
         return true;
      }
      auto index = sourceModel()->index(sourceRow, 0, sourceParent);
      auto node = dynamic_cast<WalletsViewModel*>(sourceModel())->getNode(index);

      auto wallet = node->hdWallet();
      return (wallet != nullptr) && (node->hdWallet()->walletId() == wallet_->walletId());
   }

private:
   std::shared_ptr<bs::sync::hd::Wallet> wallet_;
};

RootWalletPropertiesDialog::RootWalletPropertiesDialog(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::sync::hd::Wallet> &wallet
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SignContainer> &container
   , WalletsViewModel *walletsModel
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager> &connectionManager
   , const std::shared_ptr<AssetManager> &assetMgr
   , QWidget* parent)
  : QDialog(parent)
  , ui_(new Ui::WalletPropertiesDialog())
  , wallet_(wallet)
  , walletsManager_(walletsManager)
  , signingContainer_(container)
  , appSettings_(appSettings)
  , connectionManager_(connectionManager)
  , assetMgr_(assetMgr)
  , logger_(logger)
{
   ui_->setupUi(this);

   ui_->labelEncRank->clear();

   walletFilter_ = new CurrentWalletFilter(wallet, this);
   walletFilter_->setSourceModel(walletsModel);
   ui_->treeViewWallets->setModel(walletFilter_);

   connect(walletsModel, &WalletsViewModel::modelReset,
      this, &RootWalletPropertiesDialog::onModelReset);

   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnDescription));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnState));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnSpendableBalance));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnUnconfirmedBalance));
   ui_->treeViewWallets->hideColumn(static_cast<int>(WalletsViewModel::WalletColumns::ColumnNbAddresses));
   ui_->treeViewWallets->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui_->treeViewWallets->selectionModel(), &QItemSelectionModel::selectionChanged, this, &RootWalletPropertiesDialog::onWalletSelected);

   connect(ui_->deleteButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onDeleteWallet);
   connect(ui_->backupButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onBackupWallet);
   connect(ui_->manageEncryptionButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onChangePassword);
   connect(ui_->rescanButton, &QPushButton::clicked, this, &RootWalletPropertiesDialog::onRescanBlockchain);

   updateWalletDetails(wallet_);

   ui_->rescanButton->setEnabled(armory->state() == ArmoryConnection::State::Ready);
   ui_->manageEncryptionButton->setEnabled(false);
   walletInfo_ = bs::hd::WalletInfo(wallet_);

   if (signingContainer_) {
      if (signingContainer_->isOffline()) {
         ui_->backupButton->setEnabled(false);
         ui_->manageEncryptionButton->setEnabled(false);
      }
      connect(signingContainer_.get(), &SignContainer::QWalletInfo, this, &RootWalletPropertiesDialog::onHDWalletInfo);
      connect(signingContainer_.get(), &SignContainer::HDLeafCreated, this, &RootWalletPropertiesDialog::onHDLeafCreated);
      infoReqId_ = signingContainer_->GetInfo(wallet_->walletId());
   }

   ui_->treeViewWallets->expandAll();
}

RootWalletPropertiesDialog::~RootWalletPropertiesDialog() = default;

void RootWalletPropertiesDialog::onDeleteWallet()
{
   signingContainer_->customDialogRequest(bs::signer::ui::DialogType::DeleteWallet
                                                             , {{ QLatin1String("rootId"), walletInfo_.rootId() }});
   close();
}

void RootWalletPropertiesDialog::onBackupWallet()
{
   signingContainer_->customDialogRequest(bs::signer::ui::DialogType::BackupWallet
                                                             , {{ QLatin1String("rootId"), walletInfo_.rootId() }});
   close();
}

void RootWalletPropertiesDialog::onChangePassword()
{
   signingContainer_->customDialogRequest(bs::signer::ui::DialogType::ManageWallet
                                                             , {{ QLatin1String("rootId"), walletInfo_.rootId() }});
   close();
}

static inline QString encTypeToString(bs::wallet::EncryptionType enc)
{
   switch (enc) {
      case bs::wallet::EncryptionType::Unencrypted :
         return QObject::tr("Unencrypted");

      case bs::wallet::EncryptionType::Password :
         return QObject::tr("Password");

      case bs::wallet::EncryptionType::Auth :
         return QObject::tr("Auth eID");
   };
}

void RootWalletPropertiesDialog::onHDWalletInfo(unsigned int id, const bs::hd::WalletInfo &walletInfo)
{
   if (!infoReqId_ || (id != infoReqId_)) {
      return;
   }
   infoReqId_ = 0;

   // walletInfo arrived from sign container signal
   walletInfo_ = walletInfo;

   // but wallet name is from bs::hd::Wallet
   walletInfo_.setName(QString::fromStdString(wallet_->name()));

   ui_->manageEncryptionButton->setEnabled(true);

   if (walletsManager_->isWatchingOnly(walletInfo_.rootId().toStdString())) {
      ui_->labelEncRank->setText(tr("Watching-Only"));
   } else {
      if (walletInfo.keyRank().first == 1 && walletInfo.keyRank().second == 1) {
         if (!walletInfo.encTypes().empty()) {
            ui_->labelEncRank->setText(encTypeToString(walletInfo.encTypes().front()));
         } else {
            ui_->labelEncRank->setText(tr("Unknown"));
         }
      } else {
         ui_->labelEncRank->setText(tr("Auth eID %1 of %2").arg(walletInfo.keyRank().first).arg(walletInfo.keyRank().second));
      }
   }
}

void RootWalletPropertiesDialog::onWalletSelected()
{
   auto selection = ui_->treeViewWallets->selectionModel()->selectedIndexes();
      auto index = selection[0];

      auto modelIndex = walletFilter_->mapToSource(index);
      auto node = dynamic_cast<WalletsViewModel*>(walletFilter_->sourceModel())->getNode(modelIndex);
      const auto wallet = node->hdWallet();

      if (wallet != nullptr) {
         updateWalletDetails(wallet);
      } else {
         const auto wallets = node->wallets();
         if (wallets.size() == 1) {
            updateWalletDetails(wallets[0]);
         } else {
            updateWalletDetails(wallet_);
         }
      }
}

void RootWalletPropertiesDialog::updateWalletDetails(const std::shared_ptr<bs::sync::hd::Wallet>& wallet)
{
   ui_->labelWalletId->setText(QString::fromStdString(wallet->walletId()));
   ui_->labelWalletName->setText(QString::fromStdString(wallet->name()));
   ui_->labelDescription->setText(QString::fromStdString(wallet->description()));

   ui_->balanceWidget->hide();

   ui_->labelGroupsUsed->setText(tr("%1/%2").arg(QString::number(wallet->getNumGroups())).arg(QString::number(wallet->getNumLeaves())));
   ui_->labelAddressesActive->setText(tr("Loading..."));
   ui_->labelUTXOs->setText(tr("Loading..."));

   unsigned int nbTotalAddresses = 0;
   auto nbUTXOs = std::make_shared<std::atomic_uint>(0);
   auto nbActAddrs = std::make_shared<std::atomic_uint>(0);

   const auto &cbUTXOs = [this, nbUTXOs](std::vector<UTXO> utxos) {
      *nbUTXOs += utxos.size();
      QMetaObject::invokeMethod(this, [this, nbUTXOs] {
         ui_->labelUTXOs->setText(QString::number(*nbUTXOs));
      });
   };
   const auto &cbActiveAddrs = [this, nbActAddrs](size_t count) {
      *nbActAddrs += count;
      QMetaObject::invokeMethod(this, [this, nbActAddrs] {
         ui_->labelAddressesActive->setText(QString::number(*nbActAddrs));
      });
   };

   for (const auto &leaf : wallet->getLeaves()) {
      leaf->getSpendableTxOutList(cbUTXOs, this);
      leaf->getActiveAddressCount(cbActiveAddrs);

      nbTotalAddresses += leaf->getUsedAddressCount();
   }
   ui_->labelAddressesUsed->setText(QString::number(nbTotalAddresses));
}

void RootWalletPropertiesDialog::updateWalletDetails(const std::shared_ptr<bs::sync::Wallet>& wallet)
{
   ui_->labelWalletId->setText(QString::fromStdString(wallet->walletId()));
   ui_->labelWalletName->setText(QString::fromStdString(wallet->name()));
   ui_->labelDescription->setText(QString::fromStdString(wallet->description()));

   ui_->labelAddressesUsed->setText(QString::number(wallet->getUsedAddressCount()));

   if (wallet->isBalanceAvailable()) {
      ui_->labelAddressesActive->setText(tr("Loading..."));
      ui_->labelUTXOs->setText(tr("Loading..."));
      wallet->getSpendableTxOutList([this](std::vector<UTXO> utxos) {
         QMetaObject::invokeMethod(this, [this, size = utxos.size()] {
            ui_->labelUTXOs->setText(QString::number(size));
         });
      }, this);
      wallet->getActiveAddressCount([this](size_t count) {
         QMetaObject::invokeMethod(this, [this, count]{
            ui_->labelAddressesActive->setText(QString::number(count));
         });
      });
      ui_->labelSpendable->setText(UiUtils::displayAmount(wallet->getSpendableBalance()));
      ui_->labelUnconfirmed->setText(UiUtils::displayAmount(wallet->getUnconfirmedBalance()));
      ui_->labelTotal->setText(UiUtils::displayAmount(wallet->getTotalBalance()));
      ui_->balanceWidget->show();
   } else {
      ui_->labelAddressesActive->setText(tr("N/A"));
      ui_->labelUTXOs->setText(tr("N/A"));
      ui_->balanceWidget->hide();
   }
}

void RootWalletPropertiesDialog::startWalletScan()
{
   const auto walletsMgr = walletsManager_;
   const auto &settings = appSettings_;

   const auto &cbr = [walletsMgr](const std::string &walletId) -> unsigned int {
      const auto &wallet = walletsMgr->getWalletById(walletId);
      return wallet ? wallet->getUsedAddressCount() : 0;
   };
   const auto &cbw = [settings](const std::string &walletId, unsigned int idx) {
      settings->SetWalletScanIndex(walletId, idx);
   };

   if (wallet_->startRescan(nullptr, cbr, cbw)) {
      emit walletsManager_->walletImportStarted(wallet_->walletId());
   }
   else {
      BSMessageBox(BSMessageBox::warning, tr("Wallet rescan")
         , tr("Wallet blockchain rescan is already in progress"), this).exec();
   }
   accept();
}

void RootWalletPropertiesDialog::onRescanBlockchain()
{
   ui_->buttonBar->setEnabled(false);

   if (wallet_->isPrimary()) {
      for (const auto &cc : assetMgr_->privateShares(true)) {
         bs::hd::Path path;
         path.append(bs::hd::purpose, true);
         path.append(bs::hd::CoinType::BlockSettle_CC, true);
         path.append(cc, true);
         const auto reqId = signingContainer_->createHDLeaf(wallet_->walletId(), path);
         if (reqId) {
            createCCWalletReqs_[reqId] = cc;
         }
      }
   }
   else {
      startWalletScan();
   }
}

void RootWalletPropertiesDialog::onHDLeafCreated(unsigned int id, const std::shared_ptr<bs::sync::hd::Leaf> &leaf)
{
   if (!createCCWalletReqs_.empty() && (createCCWalletReqs_.find(id) != createCCWalletReqs_.end())) {
      const auto cc = createCCWalletReqs_[id];
      createCCWalletReqs_.erase(id);

      const auto group = wallet_->createGroup(bs::hd::CoinType::BlockSettle_CC);
      group->addLeaf(leaf);

      if (createCCWalletReqs_.empty()) {
         startWalletScan();
      }
   }
}

void RootWalletPropertiesDialog::onModelReset()
{
   ui_->treeViewWallets->expandAll();
}
