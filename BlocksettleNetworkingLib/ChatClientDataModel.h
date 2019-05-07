#ifndef CHATCLIENTUSERSMODEL_H
#define CHATCLIENTUSERSMODEL_H

#include <memory>
#include <QAbstractItemModel>

#include "ChatClientTree/TreeObjects.h"
#include "ChatHandleInterfaces.h"

class ChatClientDataModel : public QAbstractItemModel
{
public:
   enum Role {
      ItemTypeRole = Qt::UserRole + 1,
      ItemAcceptTypeRole,
      RoomTitleRole,
      RoomIdRole,
      ContactIdRole,
      ContactStatusRole,
      ContactOnlineStatusRole,
      UserIdRole,
      UserOnlineStatusRole,
      ChatNewMessageRole
   };

   ChatClientDataModel(QObject * parent = nullptr);

public:
   void clearModel();
   void clearSearch();
   bool insertRoomObject(std::shared_ptr<Chat::RoomData> data);
   bool insertContactObject(std::shared_ptr<Chat::ContactRecordData> data, bool isOnline = false);
   bool insertGeneralUserObject(std::shared_ptr<Chat::UserData> data);
   bool insertSearchUserObject(std::shared_ptr<Chat::UserData> data);
   bool insertSearchUserList(std::vector<std::shared_ptr<Chat::UserData>> userList);
   bool insertMessageNode(TreeMessageNode *messageNode);
   bool insertRoomMessage(std::shared_ptr<Chat::MessageData> message);
   bool insertContactsMessage(std::shared_ptr<Chat::MessageData> message);
   TreeItem* findChatNode(const std::string& chatId);
   std::vector<std::shared_ptr<Chat::ContactRecordData>> getAllContacts();
   bool removeContactNode(const std::string& contactId);
   std::shared_ptr<Chat::ContactRecordData> findContactItem(const std::string& contactId);
   ChatContactElement *findContactNode(const std::string& contactId);
   std::shared_ptr<Chat::MessageData> findMessageItem(const std::string& chatId, const std::string& messgeId);
   std::string currentUser() const;
   void setCurrentUser(const std::string &currentUser);
   void notifyMessageChanged(std::shared_ptr<Chat::MessageData> message);
   void notifyContactChanged(std::shared_ptr<Chat::ContactRecordData> contact);
   void setNewMessageMonitor(NewMessageMonitor* monitor);

   // QAbstractItemModel interface
public:
   QModelIndex index(int row, int column, const QModelIndex &parent) const override;
   QModelIndex parent(const QModelIndex &child) const override;
   int rowCount(const QModelIndex &parent) const override;
   int columnCount(const QModelIndex &) const override;
   QVariant data(const QModelIndex &index, int role) const override;
   Qt::ItemFlags flags(const QModelIndex &index) const override;
private slots:
   void onItemChanged(TreeItem* item);
private:
   QVariant roomData(const TreeItem * item, int role) const;
   QVariant contactData(const TreeItem * item, int role) const;
   QVariant userData(const TreeItem * item, int role) const;
   QVariant chatNewMessageData(const TreeItem * item, int role) const;

private:
   std::shared_ptr<RootItem> root_;
   void beginChatInsertRows(const TreeItem::NodeType &type);
   void updateNewMessagesFlag();

private:
   NewMessageMonitor * newMessageMonitor_;
   bool newMesagesFlag_;
};













#endif // CHATCLIENTUSERSMODEL_H
