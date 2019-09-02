#ifndef CHATPARTYLISTMODEL_H
#define CHATPARTYLISTMODEL_H

#include <QAbstractItemModel>
#include "ChatProtocol/ChatClientService.h"
#include "PartyTreeItem.h"

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

#endif // CHATPARTYLISTMODEL_H
