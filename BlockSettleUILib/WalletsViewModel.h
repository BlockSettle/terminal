/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __WALLETS_VIEW_MODEL_H__
#define __WALLETS_VIEW_MODEL_H__

#include <QAbstractItemModel>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "QWalletInfo.h"
#include "SignerDefs.h"


namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class HeadlessContainer;
class WalletsViewModel;


class WalletNode
{
public:
   enum class Type {
      Unknown,
      Root,
      WalletDummy,
      WalletPrimary,
      WalletRegular,
      GroupBitcoin,
      GroupAuth,
      GroupCC,
      Leaf
   };
   enum class State {
      Undefined,
      Primary,
      Full,
      Offline,
      Hardware
   };

   WalletNode(WalletsViewModel *vm, Type type, int row = 0, WalletNode *parent = nullptr)
      : viewModel_(vm), parent_(parent), row_(row), type_(type) {}
   virtual ~WalletNode() { clear(); }

   virtual std::vector<bs::sync::WalletInfo> wallets() const { return {}; }
   virtual bs::sync::WalletInfo hdWallet() const { return {}; }
   virtual QVariant data(int, int) const { return QVariant(); }
   virtual std::string id() const { return {}; }

   void add(WalletNode *child) { children_.append(child); }
   void remove(WalletNode* child);
   void replace(WalletNode *child);
   void clear();
   int nbChildren() const { return children_.count(); }
   bool hasChildren() const { return !children_.empty(); }
   WalletNode *parent() const { return parent_; }
   WalletNode *child(int index) const;
   int row() const { return row_; }
   void incRow(int increment = 1) { row_ += increment; }
   const std::string &name() const { return name_; }
   Type type() const { return type_; }
   State state() const { return state_; }
   virtual void setState(State state) { state_ = state; }

   WalletNode *findByWalletId(const std::string &walletId);

protected:
   std::string          name_;
   WalletsViewModel  *  viewModel_;
   WalletNode  *        parent_ = nullptr;
   int                  row_;
   Type                 type_ = Type::Unknown;
   State                state_ = State::Undefined;
   QList<WalletNode *>  children_;
};


class WalletsViewModel : public QAbstractItemModel
{
Q_OBJECT
public:
   WalletsViewModel(const std::string &defaultWalletId, QObject *parent = nullptr, bool showOnlyRegular = false);
   ~WalletsViewModel() noexcept override = default;

   WalletsViewModel(const WalletsViewModel&) = delete;
   WalletsViewModel& operator = (const WalletsViewModel&) = delete;
   WalletsViewModel(WalletsViewModel&&) = delete;
   WalletsViewModel& operator = (WalletsViewModel&&) = delete;

   std::vector<bs::sync::WalletInfo> getWallets(const QModelIndex &index) const;
   bs::sync::WalletInfo getWallet(const QModelIndex &index) const;
   WalletNode *getNode(const QModelIndex &) const;
   void setSelectedWallet(const std::string &selWallet) { selectedWalletId_ = selWallet; }

   std::string selectedWallet() const { return selectedWalletId_; }
   bool showRegularWallets() const { return showRegularWallets_; }
   std::shared_ptr<bs::sync::Wallet> getAuthWallet() const;

   [[deprecated]] void LoadWallets(bool keepSelection = false);
   void onHDWallet(const bs::sync::WalletInfo &);
   void onWalletDeleted(const bs::sync::WalletInfo&);
   void onHDWalletDetails(const bs::sync::HDWalletData &);
   void onWalletBalances(const bs::sync::WalletBalanceData &);

   void setBitcoinLeafSelectionMode(bool flag = true) { bitcoinLeafSelectionMode_ = flag; }

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex &child) const override;
   bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;

signals:
   void updateAddresses();
   void needHDWalletDetails(const std::string &walletId);
   void needWalletBalances(const std::string &walletId);

private slots:
   void onWalletChanged();
   void onNewWalletAdded(const std::string &walletId);
   void onWalletInfo(unsigned int id, bs::hd::WalletInfo);
   void onHDWalletError(unsigned int id, std::string err);
   void onSignerAuthenticated();

public:
   enum class WalletColumns : int
   {
      ColumnName,
      ColumnID,
      ColumnDescription,
      ColumnState,
      ColumnSpendableBalance,
      ColumnUnconfirmedBalance,
      ColumnTotalBalance,
      ColumnNbAddresses,
      ColumnEmpty,
      ColumnCount
   };
   enum class WalletRegColumns : int
   {
      ColumnName,
      ColumnDescription,
      ColumnState,
      ColumnNbAddresses,
      ColumnEmpty,
      ColumnCount
   };

private:
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   std::string       selectedWalletId_;
   std::shared_ptr<WalletNode>      rootNode_;
   std::string       defaultWalletId_;
   bool              showRegularWallets_;
   std::unordered_map<int, std::string>   hdInfoReqIds_;
   std::unordered_map<std::string, WalletNode::State> signerStates_;
   bool bitcoinLeafSelectionMode_ = false;
};


#endif // __WALLETS_VIEW_MODEL_H__
