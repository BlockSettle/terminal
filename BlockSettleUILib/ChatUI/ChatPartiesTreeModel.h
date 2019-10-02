#ifndef CHATPARTYLISTMODEL_H
#define CHATPARTYLISTMODEL_H

#include <QAbstractItemModel>
#include "ChatProtocol/ChatClientService.h"
#include "PartyTreeItem.h"

class OtcClient;
class ChatPartiesTreeModel : public QAbstractItemModel
{
   Q_OBJECT
public:
   ChatPartiesTreeModel(const Chat::ChatClientServicePtr& chatClientServicePtr, OtcClient *otcClient
      , QObject* parent = nullptr);
   ~ChatPartiesTreeModel() override;

   QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
   QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex& index) const override;
   int rowCount(const QModelIndex& parent = QModelIndex()) const override;
   int columnCount(const QModelIndex& parent = QModelIndex()) const override;
   const std::string& currentUser() const;

   QModelIndex getOTCGlobalRoot() const;
   const QModelIndex getPartyIndexById(const std::string& partyId, const QModelIndex parent = {}) const;

signals:
   void restoreSelectedIndex();

public slots:
   void onPartyModelChanged();
   void onGlobalOTCChanged();
   void onCleanModel();
   void onPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr);
   void onIncreaseUnseenCounter(const std::string& partyId, int newMessageCount);
   void onDecreaseUnseenCounter(const std::string& partyId, int seenMessageCount);

private:
   PartyTreeItem* getItem(const QModelIndex& index) const;
   

   PartyTreeItem* rootItem_{};

   Chat::ChatClientServicePtr chatClientServicePtr_;
   OtcClient* otcClient_{};
};

using ChatPartiesTreeModelPtr = std::shared_ptr<ChatPartiesTreeModel>;

namespace ChatModelNames {
   const QString ContainerTabGlobal = QObject::tr("Public");
   const QString ContainerTabPrivate = QObject::tr("Private");
   const QString ContainerTabContactRequest = QObject::tr("Contact request");

   // OTC
   const QString TabOTCSentRequest = QObject::tr("Send request");
   const QString TabOTCReceivedResponse = QObject::tr("Received response");
}

#endif // CHATPARTYLISTMODEL_H
