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

   //enum class PartyRoles
   //{
   //   Status = Qt::UserRole + 1,
   //   Type,
   //   SubType,
   //   Name
   //};
}

/*
class AbstractParty
{
public:
   AbstractParty(const std::string &displayName);
   virtual ~AbstractParty();

   AbstractParty(const AbstractParty& other);
   AbstractParty(AbstractParty&& other);

   void operator=(AbstractParty&& other);
   void operator=(const AbstractParty& other);

   bool operator==(const AbstractParty& other);

   virtual int rowCount() const = 0;
   virtual UI::ElementType elementType() const = 0;
   virtual QVariant data(UI::PartyRoles role) const = 0;
   virtual int row() const = 0;
   virtual AbstractParty* parentItem() = 0;
   virtual AbstractParty* childItem(int row) = 0;
   virtual int childIndex(const AbstractParty* child) const = 0;

   static int columnCount();

   // Properties
   const std::string& getDisplayName() const;
   void setDisplayName(const std::string& newName);

protected:
   std::string          displayName_;
   constexpr static int columnCount_ = 1;
};

class PartyContainer;
class Party : public AbstractParty
{
public:
   Party(const std::string& displayName, Chat::PartySubType subType, Chat::ClientStatus status);
   virtual ~Party() override;

   Party(const Party& other);
   Party(Party&& other);

   void operator=(Party &&other);
   void operator=(const Party& other);

   bool operator==(const Party& other);

   virtual int rowCount() const final;
   virtual UI::ElementType elementType() const final;
   virtual QVariant data(UI::PartyRoles role) const final;
   virtual int row() const final;
   virtual AbstractParty* parentItem() final;
   virtual AbstractParty* childItem(int row) final;
   virtual int childIndex(const AbstractParty* child) const final;

   // Properties
   Chat::ClientStatus getClientStatus() const;
   void setClientStatus(Chat::ClientStatus status);

   Chat::PartySubType getPartySubType() const;
   void setPartySubType(Chat::PartySubType subType);

   void setParent(PartyContainer* parent);

protected:
   Chat::PartySubType   subType_;
   Chat::ClientStatus   status_;
   PartyContainer*      parent_;
   constexpr static int rowCount_ = 0;
};


class PartyContainer : public AbstractParty
{
public:
   PartyContainer(Chat::PartyType partyType, const std::string& displayName, QList<Party> &&children);
   virtual ~PartyContainer() override;

   bool operator==(const PartyContainer &other);

   virtual int rowCount() const final;
   virtual UI::ElementType elementType() const final;
   virtual QVariant data(UI::PartyRoles role) const final;
   virtual int row() const final;
   virtual AbstractParty* parentItem() final;
   virtual AbstractParty* childItem(int row) final;
   virtual int childIndex(const AbstractParty* child) const final;

   // Properties
   Chat::PartyType getPartyType() const;
   void setPartyType(Chat::PartyType newPartyType);

   // children modificator
   void addParty(Party&& party);
   void removeParty(int row);
   void changeParty(int row, Party&& newParty);
   void replaceAllParties(QList<Party>&& newPartiesList);

private:
   void checkRow(int row) const;

protected:
   Chat::PartyType   partyType_;
   QList<Party>      children_;
};

//UserListView : public SomeViewQtWidget
//{
//...
//signals:
//   onLeftMouseClick(); 	// open chat window
//   onRightMouseClick();	// open item menu
//}

using PartiesList = QList<QList<Party>>;
class ChatPartiesTreeModel : public QAbstractItemModel
{
   Q_OBJECT
public:
   ChatPartiesTreeModel(Chat::ChatClientServicePtr chatClientServicePtr, QObject *parent = nullptr);
   virtual ~ChatPartiesTreeModel() override;

   // Model virtual functions
   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex& index) const override;
   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

private:
   struct FindPartyResult {
       constexpr static int iInvalid = -1;
       int iContainer_ = iInvalid;
       int iParty_ = iInvalid;
       FindPartyResult(int iContaner, int iParty);
       FindPartyResult();
       bool isValid() const;
   };
   FindPartyResult findParty(const std::string& displayName) const;
   void checkType(Chat::PartyType type) const;

   void internalReplaceAllParties(PartiesList&& newParties);
   void internalAddParty(Chat::PartyType type, Party&& party);
   void internalRemoveParty(const std::string& displayName);
   void internalChangeParty(Chat::PartyType type, Party&& newParty);

public slots:
   // All those slots should take correct data and transform it into ones for internal usage
   // for now for simplicity it's just the same interface in both cases(except slots do not work with rvalue)
   void partyModelChanged();
   //void addParty(Chat::PartyType type, const Party& party);
   //void removeParty(const std::string& displayName);
   //void changeParty(Chat::PartyType type, const Party& party);

protected:
   QScopedPointer<QList<PartyContainer>> partyContainers_;
   Chat::ChatClientServicePtr chatClientServicePtr_;
};
*/


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

private:
   QList<PartyTreeItem*> childItems_;
   QVariant itemData_;
   PartyTreeItem* parentItem_;
   UI::ElementType modelType_;
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

private slots:
   void partyModelChanged();
   void resetAll();

private:
   PartyTreeItem* getItem(const QModelIndex& index) const;

   Chat::ChatClientServicePtr chatClientServicePtr_;
   PartyTreeItem* rootItem_;
};

// #TODO: Move this code in different file
#include <QSortFilterProxyModel>

class ChatPartiesSortProxyModel : public QSortFilterProxyModel
{
   Q_OBJECT
public:
   explicit ChatPartiesSortProxyModel(QObject *parent = nullptr);

   //void setFilterKey(const QString &pattern,
   //   int role = Qt::DisplayRole,
   //   bool caseSensitive = false);

   PartyTreeItem* getInternalData(const QModelIndex& index) const;

protected:

   bool filterAcceptsRow(int row, const QModelIndex& parent) const override;
   bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

};


#endif // CHATPARTYLISTMODEL_H
