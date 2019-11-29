/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   void onGlobalOTCChanged(QMap<std::string, ReusableItemData> reusableItemData = {});
   void onCleanModel();
   void onPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr);
   void onIncreaseUnseenCounter(const std::string& partyId, int newMessageCount, bool isUnseenOTCMessage = false);
   void onDecreaseUnseenCounter(const std::string& partyId, int seenMessageCount);

private slots:
   void onUpdateOTCAwaitingColor();

private:
   PartyTreeItem* getItem(const QModelIndex& index) const;
   void forAllPartiesInModel(PartyTreeItem* parent, std::function<void(const PartyTreeItem*)>&& applyFunc) const;
   void forAllIndexesInModel(const QModelIndex& parent, std::function<void(const QModelIndex&)>&& applyFunc) const;
   QMap<std::string, ReusableItemData> collectReusableData(PartyTreeItem* parent);
   void resetOTCUnseen(const QModelIndex& parentIndex, bool isAddChildren = true, bool isClearAll = true);

   PartyTreeItem* rootItem_{};

   Chat::ChatClientServicePtr chatClientServicePtr_;
   OtcClient* otcClient_{};

   QSet<QPersistentModelIndex> otcWatchIndx_;
   QTimer otcWatchToggling_;
};

using ChatPartiesTreeModelPtr = std::shared_ptr<ChatPartiesTreeModel>;

namespace ChatModelNames {
   const QString ContainerTabGlobal = QObject::tr("Public");
   const QString ContainerTabPrivate = QObject::tr("Private");
   const QString ContainerTabContactRequest = QObject::tr("Contact request");

   const QString ContainerTabOTCIdentifier = QObject::tr("C_OTC");
   const QString ContainerTabOTCDisplayName = QObject::tr("OTC");

   const QString PrivatePartyGlobalOTCDisplayName = QObject::tr("Global");

   // OTC
   const QString TabOTCSentRequest = QObject::tr("Submitted quotes");
   const QString TabOTCReceivedResponse = QObject::tr("Received quotes");
}

#endif // CHATPARTYLISTMODEL_H
