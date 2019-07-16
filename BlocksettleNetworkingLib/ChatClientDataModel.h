#ifndef CHATCLIENTUSERSMODEL_H
#define CHATCLIENTUSERSMODEL_H

#include <memory>
#include <QAbstractItemModel>

#include "ChatClientTree/TreeObjects.h"
#include "ChatHandleInterfaces.h"
#include <spdlog/spdlog.h>

class ChatClientDataModel : public QAbstractItemModel
{
public:
   enum Role {
      ItemTypeRole = Qt::UserRole + 1,
      CategoryGroupDisplayName,
      RoomTitleRole,
      RoomIdRole,
      ContactTitleRole,
      ContactIdRole,
      ContactStatusRole,
      ContactOnlineStatusRole,
      UserIdRole,
      UserOnlineStatusRole,
      ChatNewMessageRole
   };

   ChatClientDataModel(const std::shared_ptr<spdlog::logger> &logger, QObject * parent = nullptr);

public:
   void initTreeCategoryGroup();
   void clearModel();
   void clearSearch();
   bool insertRoomObject(std::shared_ptr<Chat::Data> data);
   bool insertContactObject(std::shared_ptr<Chat::Data> data, bool isOnline = false);
   bool insertContactRequestObject(std::shared_ptr<Chat::Data> data, bool isOnline = false);
   bool insertContactCompleteObject(std::shared_ptr<Chat::Data> data, bool isOnline = false);
   bool insertGeneralUserObject(std::shared_ptr<Chat::Data> data);
   bool insertSearchUserObject(std::shared_ptr<Chat::Data> data);
   bool insertSearchUserList(std::vector<std::shared_ptr<Chat::Data>> userList);
   bool insertMessageNode(TreeMessageNode *messageNode);
   bool insertDisplayableDataNode(DisplayableDataNode * displayableNode);
   bool insertRoomMessage(std::shared_ptr<Chat::Data> message);
   bool insertContactsMessage(std::shared_ptr<Chat::Data> message);
   bool insertContactRequestMessage(std::shared_ptr<Chat::Data> message);
   TreeItem* findChatNode(const std::string& chatId);
   std::vector<std::shared_ptr<Chat::Data>> getAllContacts();
   bool removeContactNode(const std::string& contactId);
   std::string getContactDisplayName(const std::string& contactId);

   //std::string contactId copy required here
   bool removeContactRequestNode(const std::string contactId);

   std::shared_ptr<Chat::Data> findContactItem(const std::string& contactId);
   ChatContactElement *findContactNode(const std::string& contactId);
   std::shared_ptr<Chat::Data> findMessageItem(const std::string& chatId, const std::string& messgeId);
   std::string currentUser() const;
   void setCurrentUser(const std::string &currentUser);
   void notifyMessageChanged(std::shared_ptr<Chat::Data> message);
   void notifyContactChanged(std::shared_ptr<Chat::Data> contact);
   void setNewMessageMonitor(NewMessageMonitor* monitor);
   NewMessageMonitor* getNewMessageMonitor() const;

   // QAbstractItemModel interface
public:
   QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
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
   std::shared_ptr<spdlog::logger> logger_;

   std::shared_ptr<RootItem> root_;
   void beginChatInsertRows(ChatUIDefinitions::ChatTreeNodeType type);
   void updateNewMessagesFlag();

   NewMessageMonitor * newMessageMonitor_;
   ModelChangesHandler * modelChangesHandler_;
   bool newMesagesFlag_;
   std::shared_ptr<Chat::Data> lastMessage_;

   // QAbstractItemModel interface
public:
   bool setData(const QModelIndex &index, const QVariant &value, int role) override;
   void setModelChangesHandler(ModelChangesHandler *modelChangesHandler);
};

#endif // CHATCLIENTUSERSMODEL_H
