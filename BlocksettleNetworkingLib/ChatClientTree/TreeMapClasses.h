#ifndef TREEMAPCLASSES_H
#define TREEMAPCLASSES_H

#include "TreeItem.h"
#include "ChatProtocol/DataObjects.h"
class TreeMessageNode;
class ChatContactElement;
class RootItem : public TreeItem {
   friend class ChatClientDataModel;
   public:
   RootItem ()
      : TreeItem(NodeType::RootNode, NodeType::CategoryNode, NodeType::RootNode)
   {
   }

   bool insertRoomObject(std::shared_ptr<Chat::RoomData> data);
   bool insertContactObject(std::shared_ptr<Chat::ContactRecordData> data, bool isOnline = false);
   bool insertGeneralUserObject(std::shared_ptr<Chat::UserData> data);
   bool insertSearchUserObject(std::shared_ptr<Chat::UserData> data);
   TreeItem* resolveMessageTargetNode(TreeMessageNode *massageNode);
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

private:
   bool insertMessageNode(TreeMessageNode * messageNode);
   bool insertNode(TreeItem* item);
   TreeItem* findCategoryNodeWith(TreeItem::NodeType type);
   std::string currentUser_;
};

class CategoryItem : public TreeItem {
   public:
   CategoryItem(NodeType categoryType)
      : TreeItem(NodeType::CategoryNode, categoryType, NodeType::RootNode) {
   }
};

class CategoryElement : public TreeItem {
   protected:
   CategoryElement(NodeType elementType, NodeType storingType, std::shared_ptr<Chat::DataObject> object)
      : TreeItem(elementType, storingType, NodeType::CategoryNode)
      , dataObject_(object)
      , newItemsFlag_(false)
   {

   }
public:
   std::shared_ptr<Chat::DataObject> getDataObject() const {return dataObject_;}
   bool updateNewItemsFlag();
   bool getNewItemsFlag() const;
   private:
   std::shared_ptr<Chat::DataObject> dataObject_;
   bool newItemsFlag_;
};

#endif // TREEMAPCLASSES_H
