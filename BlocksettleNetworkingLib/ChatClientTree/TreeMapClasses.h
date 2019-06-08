#ifndef TREEMAPCLASSES_H
#define TREEMAPCLASSES_H

#include "TreeItem.h"

#include "ChatProtocol/DataObjects.h"

class DisplayableDataNode;
class ChatContactElement;

class RootItem : public TreeItem
{
friend class ChatClientDataModel;

public:
   RootItem ()
      : TreeItem(ChatUIDefinitions::ChatTreeNodeType::RootNode, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode}, ChatUIDefinitions::ChatTreeNodeType::RootNode)
   {
   }

   bool insertRoomObject(std::shared_ptr<Chat::RoomData> data);
   bool insertContactObject(std::shared_ptr<Chat::ContactRecordData> data, bool isOnline = false);
   bool insertGeneralUserObject(std::shared_ptr<Chat::UserData> data);
   bool insertSearchUserObject(std::shared_ptr<Chat::UserData> data);
   TreeItem* resolveMessageTargetNode(DisplayableDataNode *massageNode);
   TreeItem* findChatNode(const std::string& chatId);
   std::vector<std::shared_ptr<Chat::ContactRecordData>> getAllContacts();
   bool removeContactNode(const std::string& contactId);
   std::shared_ptr<Chat::ContactRecordData> findContactItem(const std::string& contactId);
   ChatContactElement *findContactNode(const std::string& contactId);
   std::shared_ptr<Chat::MessageData> findMessageItem(const std::string& chatId, const std::string& messgeId);
   void clear();
   void clearSearch();
   std::string currentUser() const;
   void setCurrentUser(const std::string &currentUser);
   void notifyMessageChanged(std::shared_ptr<Chat::MessageData> message);
   void notifyContactChanged(std::shared_ptr<Chat::ContactRecordData> contact);

   // insert channel for response that client send to OTC requests
   bool insertOTCSentResponseObject(const std::shared_ptr<Chat::OTCResponseData>& response);

   // insert channel for response client receive for own OTC
   bool insertOTCReceivedResponseObject(const std::shared_ptr<Chat::OTCResponseData> &response);

private:
   bool insertMessageNode(DisplayableDataNode *messageNode);
   bool insertNode(TreeItem* item);
   TreeItem* findCategoryNodeWith(ChatUIDefinitions::ChatTreeNodeType type);
   std::string currentUser_;
};

class TreeCategoryGroup : public TreeItem
{
public:
   TreeCategoryGroup(ChatUIDefinitions::ChatTreeNodeType elementType, const QString& displayName)
      : TreeItem(ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode
         , std::vector<ChatUIDefinitions::ChatTreeNodeType>{elementType}
         , ChatUIDefinitions::ChatTreeNodeType::RootNode
         , displayName)
   {
   }
};

class CategoryElement : public TreeItem
{
protected:
   CategoryElement(ChatUIDefinitions::ChatTreeNodeType elementType, const std::vector<ChatUIDefinitions::ChatTreeNodeType>& storingTypes, std::shared_ptr<Chat::DataObject> object)
      : TreeItem(elementType, storingTypes, ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode)
      , dataObject_(object)
      , newItemsFlag_(false)
   {
   }

public:
   std::shared_ptr<Chat::DataObject> getDataObject() const {return dataObject_;}
   bool updateNewItemsFlag();
   bool getNewItemsFlag() const;
protected:
   std::shared_ptr<Chat::DataObject> dataObject_;
   bool newItemsFlag_;
};

#endif // TREEMAPCLASSES_H
