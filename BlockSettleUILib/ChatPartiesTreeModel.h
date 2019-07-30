#ifndef CHATPARTYLISTMODEL_H
#define CHATPARTYLISTMODEL_H

#include <QAbstractItemModel>

// External enum
enum class StateOfParty
{
   Online = 0,
   Offline,
   // and maybe more but i need to think about it
   PartyRequest,
   PartyRejected
};

enum class PartyType
{
   Private = 0,	// Friends
   Public,		// Global chat rooms
   // and in future will be more ex.:
   OTC,			// probably this will be showing as Global chat room

   Total
};

// Internal enum
enum class ElementType
{
   Container = 0,
   Party
};

enum class PartyRoles
{
   State = Qt::UserRole + 1,
   Type,
   Name
};

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
   virtual ElementType elementType() const = 0;
   virtual QVariant data(PartyRoles role) const = 0;
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
   Party(StateOfParty state, std::string displayName);
   virtual ~Party() override;

   Party(const Party& other);
   Party(Party&& other);

   void operator=(Party &&other);
   void operator=(const Party& other);

   bool operator==(const Party& other);

   virtual int rowCount() const final;
   virtual ElementType elementType() const final;
   virtual QVariant data(PartyRoles role) const final;
   virtual int row() const final;
   virtual AbstractParty* parentItem() final;
   virtual AbstractParty* childItem(int row) final;
   virtual int childIndex(const AbstractParty* child) const final;

   // Properties
   StateOfParty getStateOfParty() const;
   void setStateOfParty(StateOfParty state);

   void setParent(PartyContainer* parent);

protected:
   StateOfParty         state_;
   PartyContainer*      parent_;
   constexpr static int rowCount_ = 0;
};


class PartyContainer : public AbstractParty
{
public:
   PartyContainer(PartyType partyType, std::string displayName, QList<Party> &&children);
   virtual ~PartyContainer() override;

   bool operator==(const PartyContainer &other);

   virtual int rowCount() const final;
   virtual ElementType elementType() const final;
   virtual QVariant data(PartyRoles role) const final;
   virtual int row() const final;
   virtual AbstractParty* parentItem() final;
   virtual AbstractParty* childItem(int row) final;
   virtual int childIndex(const AbstractParty* child) const final;

   // Properties
   PartyType getPartyType() const;
   void setPartyType(PartyType newPartyType);

   // children modificator
   void addParty(Party&& party);
   void removeParty(int row);
   void changeParty(int row, Party&& newParty);
   void replaceAllParties(QList<Party>&& newPartiesList);

private:
   void checkRow(int row) const;


protected:
   PartyType            partyType_;
   QList<Party> children_;
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

   using PartiesList = QList<QPair<PartyType, QList<Party>>>;
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
   void checkType(PartyType type) const;

   void internalReplaceAllParties(PartiesList&& newParties);
   void internalAddParty(PartyType type, Party&& party);
   void internalRemoveParty(const std::string& displayName);
   void internalChangeParty(PartyType type, Party&& newParty);

public slots:
   // All those slots should take correct data and transfrom it into ones for internal usage
   // for now for simplcity it's just the same interface in both cases(except slots do not work with rvalue)
   void replaceAllParties(const PartiesList& parties);
   void addParty(PartyType type, const Party& party);
   void removeParty(const std::string& displayName);
   void changeParty(PartyType type, const Party& party);

protected:
   QScopedPointer<QList<PartyContainer>> partyContainers_;
};

#endif // CHATPARTYLISTMODEL_H
