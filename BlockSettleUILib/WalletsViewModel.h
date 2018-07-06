#ifndef __WALLETS_VIEW_MODEL_H__
#define __WALLETS_VIEW_MODEL_H__

#include <QAbstractItemModel>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "MetaData.h"


class WalletsManager;
namespace bs {
   class Wallet;
   namespace hd {
      class Wallet;
   }
}
class SignContainer;
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
      Connected,
      Offline
   };

   WalletNode(WalletsViewModel *vm, Type type, int row = 0, WalletNode *parent = nullptr)
      : viewModel_(vm), parent_(parent), row_(row), type_(type) {}
   virtual ~WalletNode() { clear(); }

   virtual std::vector<std::shared_ptr<bs::Wallet>> wallets() const { return {}; }
   virtual std::shared_ptr<bs::hd::Wallet> hdWallet() const { return nullptr; }
   virtual QVariant data(int, int) const { return QVariant(); }
   virtual std::string id() const { return {}; }

   void add(WalletNode *child) { children_.append(child); }
   void clear();
   int nbChildren() const { return children_.count(); }
   bool hasChildren() const { return !children_.empty(); }
   WalletNode *parent() const { return parent_; }
   WalletNode *child(int index) const;
   int row() const { return row_; }
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
   WalletsViewModel(const std::shared_ptr<WalletsManager>& walletsManager, const std::string &defaultWalletId
      , const std::shared_ptr<SignContainer> &sc = nullptr, QObject *parent = nullptr, bool showOnlyRegular = false);
   ~WalletsViewModel() noexcept override = default;

   WalletsViewModel(const WalletsViewModel&) = delete;
   WalletsViewModel& operator = (const WalletsViewModel&) = delete;
   WalletsViewModel(WalletsViewModel&&) = delete;
   WalletsViewModel& operator = (WalletsViewModel&&) = delete;

   std::vector<std::shared_ptr<bs::Wallet>> getWallets(const QModelIndex &index) const;
   std::shared_ptr<bs::Wallet> getWallet(const QModelIndex &index) const;
   WalletNode *getNode(const QModelIndex &) const;
   void setSelectedWallet(const std::shared_ptr<bs::Wallet> &selWallet) { selectedWallet_ = selWallet; }

   std::shared_ptr<bs::Wallet> selectedWallet() const { return selectedWallet_; }
   bool showRegularWallets() const { return showRegularWallets_; }
   std::shared_ptr<bs::Wallet> getAuthWallet() const;

   void LoadWallets(bool keepSelection = false);

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex &child) const override;
   bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

private slots:
   void onWalletChanged();
   void onNewWalletAdded(const std::string &walletId);
   void onHDWalletInfo(unsigned int id, bs::wallet::EncryptionType, const SecureBinaryData &encKey);
   void onMissingWallets(const std::vector<std::string> &);
   void onSignerAuthenticated();

public:
   enum class WalletColumns : int
   {
      ColumnName,
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
   std::shared_ptr<WalletsManager>  walletsManager_;
   std::shared_ptr<SignContainer>   signContainer_;
   std::shared_ptr<bs::Wallet>      selectedWallet_;
   std::shared_ptr<WalletNode>      rootNode_;
   std::string       defaultWalletId_;
   bool              showRegularWallets_;
   std::unordered_map<int, std::string>   hdInfoReqIds_;
   std::unordered_set<std::string>        failedLeaves_;
   std::unordered_map<std::string, WalletNode::State> signerStates_;
};


class QmlWalletsViewModel : public WalletsViewModel
{
   Q_OBJECT
public:
   QmlWalletsViewModel(const std::shared_ptr<WalletsManager>& walletsMgr, QObject *parent = nullptr)
      : WalletsViewModel(walletsMgr, "", nullptr, parent) {}
   ~QmlWalletsViewModel() override = default;

   enum Roles {
      firstRole = Qt::UserRole + 1,
      NameRole = firstRole,
      DescRole,
      StateRole,
      WalletIdRole,
      IsHDRootRole,
      IsEncryptedRole
   };
   Q_ENUM(Roles)

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
   QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;
};

#endif // __WALLETS_VIEW_MODEL_H__
