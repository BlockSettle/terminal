/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   , walletInfo_(walletsManager_, wallet_)
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

   ui_->rescanButton->setEnabled(armory->state() == ArmoryState::Ready);
   ui_->manageEncryptionButton->setEnabled(false);

   if (signingContainer_) {
      if (signingContainer_->isOffline()) {
         ui_->backupButton->setEnabled(false);
         ui_->manageEncryptionButton->setEnabled(false);
      }
      connect(signingContainer_.get(), &SignContainer::QWalletInfo, this, &RootWalletPropertiesDialog::onHDWalletInfo);

      infoReqId_ = signingContainer_->GetInfo(wallet_->walletId());
   }

   ui_->treeViewWallets->expandAll();
}

RootWalletPropertiesDialog::~RootWalletPropertiesDialog() = default;

void RootWalletPropertiesDialog::onDeleteWallet()
{
   signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::DeleteWallet
                                                             , {{ QLatin1String("rootId"), walletInfo_.rootId() }});
   close();
}

void RootWalletPropertiesDialog::onBackupWallet()
{
   signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::BackupWallet
                                                             , {{ QLatin1String("rootId"), walletInfo_.rootId() }});
   close();
}

void RootWalletPropertiesDialog::onChangePassword()
{
   signingContainer_->customDialogRequest(bs::signer::ui::GeneralDialogType::ManageWallet
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

   //no default entry in switch statment nor default return value
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
      if (walletInfo.keyRank().m == 1 && walletInfo.keyRank().n == 1) {
         if (!walletInfo.encTypes().empty()) {
            ui_->labelEncRank->setText(encTypeToString(walletInfo.encTypes().front()));
         } else {
            ui_->labelEncRank->setText(tr("Unknown"));
         }
      } else {
         ui_->labelEncRank->setText(tr("Auth eID %1 of %2").arg(walletInfo.keyRank().m).arg(walletInfo.keyRank().n));
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

   QPointer<RootWalletPropertiesDialog> thisPtr = this;
   const auto &cbUTXOs = [thisPtr, nbUTXOs](const std::vector<UTXO> &utxos) {
      *nbUTXOs += uint32_t(utxos.size());
      QMetaObject::invokeMethod(qApp, [thisPtr, nbUTXOs] {
         if (thisPtr) {
            thisPtr->ui_->labelUTXOs->setText(QString::number(*nbUTXOs));
         }
      });
   };

   for (const auto &leaf : wallet->getLeaves()) 
   {
      leaf->getSpendableTxOutList(cbUTXOs, UINT64_MAX);

      auto addrCnt = leaf->getActiveAddressCount();
      ui_->labelAddressesActive->setText(QString::number(addrCnt));

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
      }, UINT64_MAX);
      
      auto addrCnt = wallet->getActiveAddressCount();
      QMetaObject::invokeMethod(this, [this, addrCnt] {
            ui_->labelAddressesActive->setText(QString::number(addrCnt));
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

void RootWalletPropertiesDialog::onRescanBlockchain()
{
   ui_->buttonBar->setEnabled(false);

/*   if (wallet_->isPrimary()) {
      for (const auto &cc : assetMgr_->privateShares(true)) {
         bs::hd::Path path;
         path.append(bs::hd::purpose | 0x80000000);
         path.append(bs::hd::CoinType::BlockSettle_CC | 0x80000000);
         path.append(cc);
         const auto reqId = signingContainer_->createHDLeaf(wallet_->walletId(), path);
         if (reqId) {
            createCCWalletReqs_[reqId] = cc;
         }
      }
   }
   else {
      startWalletScan();
   }*/
   wallet_->startRescan();
   accept();
}

void RootWalletPropertiesDialog::onModelReset()
{
   ui_->treeViewWallets->expandAll();
}
