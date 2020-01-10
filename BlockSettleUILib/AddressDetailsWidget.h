/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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

#include <QWidget>
#include <QItemSelection>

namespace Ui {
   class AddressDetailsWidget;
}
namespace bs {
   namespace sync {
      class CCDataResolver;
      class PlainWallet;
   }
}
class AddressVerificator;
class QTreeWidgetItem;

class AddressDetailsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit AddressDetailsWidget(QWidget *parent = nullptr);
   ~AddressDetailsWidget() override;

   void init(const std::shared_ptr<ArmoryConnection> &armory
      , const std::shared_ptr<spdlog::logger> &inLogger
      , const std::shared_ptr<bs::sync::CCDataResolver> &);
   void setQueryAddr(const bs::Address& inAddrVal);
   void setBSAuthAddrs(const std::unordered_set<std::string> &bsAuthAddrs);
   void clear();

   enum AddressTreeColumns {
      colDate = 0,
      colTxId = 1,
      colConfs = 2,
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

private slots:
   void onTxClicked(QTreeWidgetItem *item, int column);
   void OnRefresh(std::vector<BinaryData> ids, bool online);
   void updateFields();

private:
   void setConfirmationColor(QTreeWidgetItem *item);
   void setOutputColor(QTreeWidgetItem *item);
   void getTxData(const std::shared_ptr<AsyncClient::LedgerDelegate> &);
   void refresh(const std::shared_ptr<bs::sync::PlainWallet> &);
   void loadTransactions();
   void searchForCC();
   void searchForAuth();

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

   std::unique_ptr<Ui::AddressDetailsWidget> ui_; // The main widget object.
   bs::Address    currentAddr_;
   bool           balanceLoaded_ = false;
   std::atomic_uint64_t totalSpent_{};
   std::atomic_uint64_t totalReceived_{};
   std::unordered_map<std::string, std::shared_ptr<bs::sync::PlainWallet>> dummyWallets_;
   std::map<BinaryData, Tx> txMap_; // A wallet's Tx hash / Tx map.
   std::map<BinaryData, bs::TXEntry> txEntryHashSet_; // A wallet's Tx hash / Tx entry map.

   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::sync::CCDataResolver> ccResolver_;
   std::pair<std::string, uint64_t>    ccFound_;
   std::shared_ptr<AddressVerificator> addrVerify_;
   std::map<bs::Address, AddressVerificationState> authAddrStates_;
   std::unordered_set<std::string>     bsAuthAddrs_;

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
