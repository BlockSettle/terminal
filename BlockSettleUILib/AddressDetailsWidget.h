/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ADDRESSDETAILSWIDGET_H__
#define __ADDRESSDETAILSWIDGET_H__

#include "Address.h"
#include "AuthAddress.h"
#include "ArmoryConnection.h"
#include "SignerDefs.h"

#include <QWidget>
#include <QItemSelection>

namespace Ui {
   class AddressDetailsWidget;
}
namespace bs {
   namespace sync {
      class CCDataResolver;
      class PlainWallet;
      class WalletsManager;
   }
}
class AddressVerificator;
class ColoredCoinTrackerClient;
class QTreeWidgetItem;


class AddressDetailsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit AddressDetailsWidget(QWidget *parent = nullptr);
   ~AddressDetailsWidget() override;

   [[deprecated]] void init(const std::shared_ptr<ArmoryConnection> &armory
      , const std::shared_ptr<spdlog::logger> &inLogger
      , const std::shared_ptr<bs::sync::CCDataResolver> &
      , const std::shared_ptr<bs::sync::WalletsManager> &);
   void init(const std::shared_ptr<spdlog::logger>& inLogger);

   void setQueryAddr(const bs::Address& inAddrVal);
   void setBSAuthAddrs(const std::unordered_set<std::string> &bsAuthAddrs);
   void clear();

   void onNewBlock(unsigned int blockNum);
   void onAddressHistory(const bs::Address&, uint32_t curBlock
      , const std::vector<bs::TXEntry>&);
   void onTXDetails(const std::vector<bs::sync::TXWalletDetails>&);

   enum AddressTreeColumns {
      colDate,
      colTxId,
      colConfs,
      colInputsNum,
      colOutputsNum,
      colOutputAmt,
      colFees,
      colFeePerByte,
      colTxSize
   };

signals:
   void transactionClicked(QString txId);
   void finished() const;
   void needAddressHistory(const bs::Address&);
   void needTXDetails(const std::vector<bs::sync::TXWallet>&, bool useCache
      , const bs::Address &);

private slots:
   void onTxClicked(QTreeWidgetItem *item, int column);
   void OnRefresh(std::vector<BinaryData> ids, bool online);   //deprecated
   void updateFields();

private:
   void setConfirmationColor(QTreeWidgetItem *item);
   [[deprecated]] void getTxData(const std::shared_ptr<AsyncClient::LedgerDelegate> &);
   [[deprecated]] void refresh(const std::shared_ptr<bs::sync::PlainWallet> &);
   [[deprecated]] void loadTransactions();
   [[deprecated]] void searchForCC();
   [[deprecated]] void searchForAuth();

private:
   // NB: Right now, the code is slightly inefficient. There are two maps with
   // hashes for keys. One has transactions (Armory), and TXEntry objects (BS).
   // This is due to the manner in which we retrieve data from Armory. Pages are
   // returned for addresses, and we then retrieve the appropriate Tx objects
   // from Armory. (Tx searches go directly to Tx object retrieval.) The thing
   // is that the pages are what have data related to # of confs and other
   // block-related data. The Tx objects from Armory don't have block-related
   // data that we need. So, we need two maps, at least for now.
   //
   // In addition, note that the TX hashes returned by Armory are in "internal"
   // byte order, whereas the displayed values need to be in "RPC" byte order.
   // (Look at the BinaryTXID class comments for more info on this phenomenon.)
   // The only time we care about this is when displaying data to the user; the
   // data is consistent otherwise, which makes Armory happy. Don't worry about
   // about BinaryTXID. A simple endian flip in printed strings is all we need.

   struct CcData
   {
      std::shared_ptr<ColoredCoinTrackerClient> tracker;
      std::string security;
      uint64_t lotSize{};
      uint64_t ccBalance{};
      bool isGenesisAddr{};
   };

   std::unique_ptr<Ui::AddressDetailsWidget> ui_; // The main widget object.
   bs::Address    currentAddr_;
   std::string    currentAddrStr_;
   std::int64_t totalSpent_{};
   std::int64_t totalReceived_{};
   std::unordered_map<std::string, std::shared_ptr<bs::sync::PlainWallet>> dummyWallets_;
   AsyncClient::TxBatchResult txMap_; // A wallet's Tx hash / Tx map.
   std::map<BinaryData, bs::TXEntry> txEntryHashSet_; // A wallet's Tx hash / Tx entry map.

   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::sync::CCDataResolver> ccResolver_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   CcData ccFound_;
   std::shared_ptr<AddressVerificator> addrVerify_;
   std::map<bs::Address, AddressVerificationState> authAddrStates_;
   std::unordered_set<std::string>     bsAuthAddrs_;
   bool isAuthAddr_{false};
   uint32_t topBlock_{ 0 };

   std::mutex mutex_;

   class AddrDetailsACT : public ArmoryCallbackTarget
   {
   public:
      AddrDetailsACT(AddressDetailsWidget *parent)
         : parent_(parent) {}
      ~AddrDetailsACT() override { cleanup(); }
      void onRefresh(const std::vector<BinaryData> &ids, bool online) override {
         QMetaObject::invokeMethod(parent_, [this, ids, online] {
            parent_->OnRefresh(ids, online);
         });
      }
   private:
      AddressDetailsWidget *parent_;
   };
   std::unique_ptr<AddrDetailsACT>  act_;
};

#endif // ADDRESSDETAILSWIDGET_H
