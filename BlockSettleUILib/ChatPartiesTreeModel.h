#ifndef CHATPARTYLISTMODEL_H
#define CHATPARTYLISTMODEL_H

#include <QAbstractItemModel>
#include "../BlocksettleNetworkingLib/ChatProtocol/Party.h"
#include "ChatProtocol/ChatClientService.h"
#include "chat.pb.h"
/*
// Internal enum
namespace UI {
   enum class ElementType
   {
      Container = 0,
      Party
   };

   enum class PartyRoles
   {
      Status = Qt::UserRole + 1,
      Type,
      SubType,
      Name
   };
}

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
   Party(std::string displayName, Chat::PartySubType subType, Chat::ClientStatus status);
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
   PartyContainer(Chat::PartyType partyType, std::string displayName, QList<Party> &&children);
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

class ChatPartiesTreeModel : public QAbstractItemModel
{
   Q_OBJECT
public:
   ChatPartiesTreeModel(QObject *parent = nullptr);
   virtual ~ChatPartiesTreeModel() override;

   // Model virtual functions
   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex& index) const override;
   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;

   using PartiesList = QList<QPair<Chat::PartyType, QList<Party>>>;
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
   // All those slots should take correct data and transfrom it into ones for internal usage
   // for now for simplcity it's just the same interface in both cases(except slots do not work with rvalue)
   void replaceAllParties(const PartiesList& parties);
   void addParty(Chat::PartyType type, const Party& party);
   void removeParty(const std::string& displayName);
   void changeParty(Chat::PartyType type, const Party& party);

protected:
   QScopedPointer<QList<PartyContainer>> partyContainers_;
};
*/

class PartyTreeItem
{
public:
   PartyTreeItem(const QVariant& data, PartyTreeItem* parent = nullptr)
   {
      parentItem = parent;
      itemData = data;
   }

   ~PartyTreeItem()
   {
      qDeleteAll(childItems);
   }

   PartyTreeItem* child(int number)
   {
      return childItems.value(number);
   }

   int childCount() const
   {
      return childItems.count();
   }

   int columnCount() const
   {
      return 1;
   }

   QVariant data() const
   {
      return itemData;
   }

   bool insertChildren(PartyTreeItem* item)
   {
      childItems.push_back(item);
      return true;
   }

   PartyTreeItem* parent()
   {
      return parentItem;
   }

   bool removeChildren(int position, int count)
   {
      if (position < 0 || position + count > childItems.size())
         return false;

      for (int row = 0; row < count; ++row)
         delete childItems.takeAt(position);

      return true;
   }

   int childNumber() const
   {
      if (parentItem)
         return parentItem->childItems.indexOf(const_cast<PartyTreeItem*>(this));

      return 0;
   }

   bool setData(const QVariant& value)
   {
      itemData = value;
      return true;
   }

private:
   QList<PartyTreeItem*> childItems;
   QVariant itemData;
   PartyTreeItem* parentItem;
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

public slots:
   void partyModelChanged();

private:
   PartyTreeItem* getItem(const QModelIndex& index) const;

   Chat::ChatClientServicePtr chatClientServicePtr_;
   PartyTreeItem* rootItem_;
};

#endif // CHATPARTYLISTMODEL_H
