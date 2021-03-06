/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QmlWalletsViewModel.h"
#include <QFont>
#include <QTreeView>
#include <QSortFilterProxyModel>
#include "Wallets/SyncHDWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "UiUtils.h"


void QmlWalletNode::clear()
{
   qDeleteAll(children_);
   children_.clear();
}

QmlWalletNode *QmlWalletNode::child(int index) const
{
   return ((index >= nbChildren()) || (index < 0)) ? nullptr : children_[index];
}

QmlWalletNode *QmlWalletNode::findByWalletId(const std::string &walletId)
{
   if ((type() == Type::Leaf) && (wallets()[0] != nullptr) && (wallets()[0]->walletId() == walletId)) {
      return this;
   }
   for (const auto &child : children_) {
      auto node = child->findByWalletId(walletId);
      if (node != nullptr) {
         return node;
      }
   }
   return nullptr;
}

class QmlWalletRootNode : public QmlWalletNode
{
public:
   QmlWalletRootNode(QmlWalletsViewModel *vm, const std::string &name, const std::string &desc
      , QmlWalletNode::Type type, bool isWO, bool isHw , int row, QmlWalletNode *parent)
      : QmlWalletNode(vm, type, isWO, isHw, row, parent), desc_(desc)
   {
      name_ = name;
   }

   QVariant data(int col, int role) const override {
      if (role == Qt::DisplayRole) {
         switch (static_cast<QmlWalletsViewModel::WalletColumns>(col)) {
         case QmlWalletsViewModel::WalletColumns::ColumnName:
            return QString::fromStdString(name_);
         case QmlWalletsViewModel::WalletColumns::ColumnDescription:
            return type() == Type::WalletRegular || type() == Type::WalletPrimary ? QString::fromStdString(desc_) : QVariant();
         case QmlWalletsViewModel::WalletColumns::ColumnID:
            return QString::fromStdString(id());
         case QmlWalletsViewModel::WalletColumns::ColumnType:
            return isHw() ? QObject::tr("HW")
               : ( isWO() ? QObject::tr("Watching-Only")
               : QObject::tr("Full") );
         default:
            return QVariant();
         }
      }
      return QVariant();
   }

   std::string id() const override {
      return (hdWallet_ ? hdWallet_->walletId() : std::string{});
   }

   void addGroups(const std::vector<std::shared_ptr<bs::sync::hd::Group>> &groups);

   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets() const override
   {
      std::vector<std::shared_ptr<bs::sync::Wallet>> ret = wallets_;

      for (const auto * g : qAsConst(children_)) {
         const auto tmp = g->wallets();
         ret.insert(ret.end(), tmp.cbegin(), tmp.cend());
      }

      return ret;
   }
   std::shared_ptr<bs::sync::hd::Wallet> hdWallet() const override { return hdWallet_; }

   void setHdWallet(const std::shared_ptr<bs::sync::hd::Wallet> &hdWallet) {
      hdWallet_ = hdWallet;
   }

protected:
   std::string desc_;
   std::shared_ptr<bs::sync::hd::Wallet>           hdWallet_;
   std::vector<std::shared_ptr<bs::sync::Wallet>>  wallets_;

protected:
   static QmlWalletNode::Type getNodeType(bs::core::wallet::Type grpType) {
      switch (grpType) {
      case bs::core::wallet::Type::Bitcoin:
         return Type::GroupBitcoin;

      case bs::core::wallet::Type::Authentication:
         return Type::GroupAuth;

      case bs::core::wallet::Type::ColorCoin:
         return Type::GroupCC;

      default:
         return Type::Unknown;
      }
   }
};

class QmlWalletLeafNode : public QmlWalletRootNode
{
public:
   QmlWalletLeafNode(QmlWalletsViewModel *vm, const std::shared_ptr<bs::sync::Wallet> &wallet, bool isWO, bool isHw
      , int row, QmlWalletNode *parent)
      : QmlWalletRootNode(vm, wallet->shortName(), wallet->description(), Type::Leaf, isWO, isHw ,row, parent)
      , wallet_(wallet)
   { }

   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets() const override { return {wallet_}; }

   std::string id() const override {
      return wallet_->walletId();
   }

   QVariant data(int col, int role) const override {
      return QmlWalletRootNode::data(col, role);
   }

private:
   std::shared_ptr<bs::sync::Wallet>   wallet_;
};

class QmlWalletGroupNode : public QmlWalletRootNode
{
public:
   QmlWalletGroupNode(QmlWalletsViewModel *vm, const std::string &name, const std::string &desc
      , QmlWalletNode::Type type, bool isWO, bool isHw, int row, QmlWalletNode *parent)
      : QmlWalletRootNode(vm, name, desc, type, isWO, isHw, row, parent) {}

   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets() const override { return wallets_; }

   void addLeaves(const std::vector<std::shared_ptr<bs::sync::Wallet>> &leaves) {
      for (const auto &leaf : leaves) {
         const auto leafNode = new QmlWalletLeafNode(viewModel_, leaf, isWO_, isHw_, nbChildren(), this);
         add(leafNode);
         wallets_.push_back(leaf);
      }
   }
};

void QmlWalletRootNode::addGroups(const std::vector<std::shared_ptr<bs::sync::hd::Group>> &groups)
{
   for (const auto &group : groups) {
      const auto groupNode = new QmlWalletGroupNode(viewModel_, group->name(), {}
         , getNodeType(group->type()), isWO_, isHw_, nbChildren(), this);
      add(groupNode);
      groupNode->addLeaves(group->getAllLeaves());
   }
}


QmlWalletsViewModel::QmlWalletsViewModel(QObject* parent)
   : QAbstractItemModel(parent)
{
   rootNode_ = std::make_shared<QmlWalletNode>(this, QmlWalletNode::Type::Root);
}

void QmlWalletsViewModel::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   walletsManager_ = walletsManager;
   loadWallets("");

   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletIsReady, this, &QmlWalletsViewModel::loadWallets);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, this, &QmlWalletsViewModel::loadWallets);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletAdded, this, &QmlWalletsViewModel::loadWallets);
}

QmlWalletNode *QmlWalletsViewModel::getNode(const QModelIndex &index) const
{
   if (!index.isValid()) {
      return rootNode_.get();
   }
   return static_cast<QmlWalletNode*>(index.internalPointer());
}

int QmlWalletsViewModel::columnCount(const QModelIndex &) const
{
   return static_cast<int>(WalletColumns::ColumnCount);
}

int QmlWalletsViewModel::rowCount(const QModelIndex &parent) const
{
   return getNode(parent)->nbChildren();
}

std::shared_ptr<bs::sync::Wallet> QmlWalletsViewModel::getWallet(const QModelIndex &index) const
{
   const auto node = getNode(index);
   if (!node) {
      return nullptr;
   }
   const auto wallets = node->wallets();
   if (wallets.size() == 1) {
      return wallets[0];
   }
   return nullptr;
}

QVariant QmlWalletsViewModel::getData(const QModelIndex &index, int role) const
{
   return getNode(index)->data(index.column(), role);
}

QVariant QmlWalletsViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role == Qt::DisplayRole) {
      switch (static_cast<WalletColumns>(section)) {
      case WalletColumns::ColumnDescription:
         return tr("Description");
      case WalletColumns::ColumnName:
         return tr("Name");
      case WalletColumns::ColumnID:
         return tr("ID");
      case WalletColumns::ColumnType:
         return tr("Type");
      default:
         return QVariant();
      }
   }

   return QVariant();
}

QModelIndex QmlWalletsViewModel::index(int row, int column, const QModelIndex &parent) const
{
   if (!hasIndex(row, column, parent)) {
      return QModelIndex();
   }

   auto node = getNode(parent);
   auto child = node->child(row);
   if (child == nullptr) {
      return QModelIndex();
   }
   return createIndex(row, column, static_cast<void*>(child));
}

QModelIndex QmlWalletsViewModel::parent(const QModelIndex &child) const
{
   if (!child.isValid()) {
      return QModelIndex();
   }

   auto node = getNode(child);
   auto parentNode = (node == nullptr) ? nullptr : node->parent();
   if ((parentNode == nullptr) || (parentNode == rootNode_.get())) {
      return QModelIndex();
   }
   return createIndex(parentNode->row(), 0, static_cast<void*>(parentNode));
}

bool QmlWalletsViewModel::hasChildren(const QModelIndex& parent) const
{
   auto node = getNode(parent);
   return node->hasChildren();
}

static QmlWalletNode::Type getHDWalletType(const std::shared_ptr<bs::sync::hd::Wallet> &hdWallet
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   if (walletsMgr->getPrimaryWallet() == hdWallet) {
      return QmlWalletNode::Type::WalletPrimary;
   }
   return QmlWalletNode::Type::WalletRegular;
}

void QmlWalletsViewModel::loadWallets(const std::string &)
{
   beginResetModel();
   rootNode_->clear();
   for (const auto &hdWallet : walletsManager_->hdWallets()) {
      if (!hdWallet) {
         continue;
      }
      const auto walletType = getHDWalletType(hdWallet, walletsManager_);
      const bool isWO = walletsManager_->isWatchingOnly(hdWallet->walletId());
      const bool isHw = hdWallet->isHardwareWallet();
      const auto hdNode = new QmlWalletRootNode(this, hdWallet->name(), hdWallet->description()
         , walletType, isWO, isHw, rootNode_->nbChildren(), rootNode_.get());
      hdNode->setHdWallet(hdWallet);
      rootNode_->add(hdNode);
      hdNode->addGroups(hdWallet->getGroups());
   }

   endResetModel();
}

QVariant QmlWalletsViewModel::data(const QModelIndex &index, int role) const
{
   if (index.isValid() && role >= firstRole) {
      const auto &node = getNode(index);
      if (!node) {
         return tr("invalid node");
      }
      auto hdWallet = node->hdWallet();
      if (node->type() == QmlWalletNode::Type::Leaf) {
         auto parent = node->parent();
         if (parent) {
            parent = parent->parent();
         }
         hdWallet = parent ? parent->hdWallet() : nullptr;
      }
      const bool isGroup = ((node->type() == QmlWalletNode::Type::GroupBitcoin) || (node->type() == QmlWalletNode::Type::GroupAuth)
         || (node->type() == QmlWalletNode::Type::GroupCC));

      switch (role) {
      case NameRole:       return node->data(static_cast<int>(WalletColumns::ColumnName), Qt::DisplayRole);
      case DescRole:       return node->data(static_cast<int>(WalletColumns::ColumnDescription), Qt::DisplayRole);
      case WalletIdRole:   return QString::fromStdString(node->id());
      case IsEncryptedRole: return hdWallet ? static_cast<int>(hdWallet->encryptionTypes().empty() ? bs::wallet::EncryptionType::Unencrypted : hdWallet->encryptionTypes()[0]) : 0;
      case EncKeyRole:     return hdWallet ? (hdWallet->encryptionKeys().empty() ? QString() : QString::fromStdString(hdWallet->encryptionKeys()[0].toBinStr())) : QString();
      case StateRole:
         if ((node->type() != QmlWalletNode::Type::Leaf) || !hdWallet) {
            return {};
         }
         if (hdWallet->encryptionTypes().empty()) {
            return tr("No");
         }
         else if (hdWallet->encryptionRank().m <= 1) {
            switch (hdWallet->encryptionTypes()[0]) {
            case bs::wallet::EncryptionType::Password:   return tr("Password");
            case bs::wallet::EncryptionType::Auth:   return tr("Auth eID");
            case bs::wallet::EncryptionType::Hardware:   return tr("Hardware Wallet");
            default:    return tr("No");
            }
         }
         else {
            return tr("%1 of %2").arg(hdWallet->encryptionRank().m).arg(hdWallet->encryptionRank().n);
         }
      case RootWalletIdRole:  return hdWallet ? QString::fromStdString(hdWallet->walletId()) : QString();
      case IsHDRootRole:   return ((node->type() == QmlWalletNode::Type::WalletPrimary)
                                 || (node->type() == QmlWalletNode::Type::WalletRegular));
      case WalletTypeRole: return (isGroup ? QVariant() : node->data(static_cast<int>(WalletColumns::ColumnType), Qt::DisplayRole));
      default:    break;
      }
   }
   return getData(index, role);
}

QHash<int, QByteArray> QmlWalletsViewModel::roleNames() const
{
   return QHash<int, QByteArray> {
      { NameRole, QByteArrayLiteral("name") },
      { DescRole, QByteArrayLiteral("desc") },
      { StateRole, QByteArrayLiteral("state") },
      { WalletIdRole, QByteArrayLiteral("walletId") },
      { IsHDRootRole, QByteArrayLiteral("isHdRoot") },
      { RootWalletIdRole, QByteArrayLiteral("rootWalletId") },
      { IsEncryptedRole, QByteArrayLiteral("isEncrypted") },
      { EncKeyRole, QByteArrayLiteral("encryptionKey") },
      { WalletTypeRole, QByteArrayLiteral("walletType") }
   };
}
