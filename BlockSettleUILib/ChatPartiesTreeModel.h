#ifndef CHATPARTYLISTMODEL_H
#define CHATPARTYLISTMODEL_H

#include <QAbstractItemModel>
#include "../BlocksettleNetworkingLib/ChatProtocol/Party.h"
#include "ChatProtocol/ChatClientService.h"
#include "chat.pb.h"
// Internal enum
namespace UI {
   enum class ElementType
   {
      Root = 0,
      Container,
      Party
   };
}

class PartyTreeItem
{
public:
   PartyTreeItem(const QVariant& data, UI::ElementType modelType, PartyTreeItem* parent = nullptr)
      : itemData_(data)
      , modelType_(modelType)
      , parentItem_(parent)
   {
   }

   ~PartyTreeItem()
   {
      qDeleteAll(childItems_);
   }

   PartyTreeItem* child(int number)
   {
      Q_ASSERT(number >= 0 && number < childItems_.size());
      return childItems_.value(number);
   }

   int childCount() const
   {
      return childItems_.count();
   }

   int columnCount() const
   {
      return 1;
   }

   QVariant data() const
   {
      return itemData_;
   }

   bool insertChildren(PartyTreeItem* item)
   {
      childItems_.push_back(item);
      return true;
   }

   PartyTreeItem* parent()
   {
      return parentItem_;
   }

   bool removeChildren(int position, int count)
   {
      if (position < 0 || position + count > childItems_.size()) {
         return false;
      }

      for (int row = 0; row < count; ++row)
         delete childItems_.takeAt(position);

      return true;
   }

   void removeAll() {
      qDeleteAll(childItems_);
      childItems_.clear();
   }

   int childNumber() const
   {
      if (parentItem_) {
         return parentItem_->childItems_.indexOf(const_cast<PartyTreeItem*>(this));
      }

      Q_ASSERT(false);
      return 0;
   }

   bool setData(const QVariant& value)
   {
      itemData_ = value;
      return true;
   }

   UI::ElementType modelType() const 
   {
      return modelType_;
   }

   void increaseUnreadedCounter(int newMessageCount) {
      Q_ASSERT(newMessageCount > 0);
      unreadedCounter_ += newMessageCount;
   }

   void decreaseUnreadedCounter(int seenMessageCount) {
      unreadedCounter_ -= seenMessageCount;
      unreadedCounter_ = std::max(unreadedCounter_, 0);
   }

   bool hasNewMessages() const {
      return unreadedCounter_ > 0;
   }

private:
   QList<PartyTreeItem*> childItems_;
   QVariant itemData_;
   PartyTreeItem* parentItem_;
   UI::ElementType modelType_;
   int unreadedCounter_ = 0;
};

class ChatPartiesTreeModel : public QAbstractItemModel
{
   Q_OBJECT
public:
   ChatPartiesTreeModel(const Chat::ChatClientServicePtr& chatClientServicePtr, QObject* parent = nullptr);
   ~ChatPartiesTreeModel();

   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex& index) const override;
   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;
   const std::string& currentUser() const;

public slots:
   void partyModelChanged();
   void cleanModel();
   void partyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr);
   void increaseUnseenCounter(const std::string& partyId, int newMessageCount);
   void decreaseUnseenCounter(const std::string& partyId, int seenMessageCount);

   const QModelIndex getPartyIndexById(const std::string& partyId) const;

private:
   PartyTreeItem* getItem(const QModelIndex& index) const;

   Chat::ChatClientServicePtr chatClientServicePtr_;
   PartyTreeItem* rootItem_;
};

using ChatPartiesTreeModelPtr = std::shared_ptr<ChatPartiesTreeModel>;

// #new_logic: Move this code in different file
#include <QSortFilterProxyModel>
class ChatPartiesSortProxyModel : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   explicit ChatPartiesSortProxyModel(ChatPartiesTreeModelPtr sourceModel, QObject *parent = nullptr);

   PartyTreeItem* getInternalData(const QModelIndex& index) const;

   const std::string& currentUser() const;

   QModelIndex getProxyIndexById(const std::string& partyId) const;

protected:

   bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
   bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
   ChatPartiesTreeModelPtr sourceModel_;
};

using ChatPartiesSortProxyModelPtr = std::shared_ptr<ChatPartiesSortProxyModel>;

#endif // CHATPARTYLISTMODEL_H
