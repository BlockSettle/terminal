#ifndef TREEMAPCLASSES_H
#define TREEMAPCLASSES_H

#include <memory>
#include "TreeItem.h"
#include "chat.pb.h"

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

   bool insertRoomObject(std::shared_ptr<Chat::Data> data);
   bool insertContactObject(std::shared_ptr<Chat::Data> data, bool isOnline = false);
   bool insertContactRequestObject(std::shared_ptr<Chat::Data> data, bool isOnline = false);
   bool insertGeneralUserObject(std::shared_ptr<Chat::Data> data);
   bool insertSearchUserObject(std::shared_ptr<Chat::Data> data);
   TreeItem* resolveMessageTargetNode(DisplayableDataNode *massageNode);
   TreeItem* findChatNode(const std::string& chatId);
   std::vector<std::shared_ptr<Chat::Data>> getAllContacts();
   bool removeContactNode(const std::string& contactId);
   bool removeContactRequestNode(const std::string& contactId);
   std::shared_ptr<Chat::Data> findContactItem(const std::string& contactId);
   ChatContactElement *findContactNode(const std::string& contactId);
   std::shared_ptr<Chat::Data> findMessageItem(const std::string& chatId, const std::string& messgeId);
   void clear();
   void clearSearch();
   std::string currentUser() const;
   void setCurrentUser(const std::string &currentUser);
   void notifyMessageChanged(std::shared_ptr<Chat::Data> message);
   void notifyContactChanged(std::shared_ptr<Chat::Data> contact);

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
   CategoryElement(ChatUIDefinitions::ChatTreeNodeType elementType, const std::vector<ChatUIDefinitions::ChatTreeNodeType>& storingTypes, std::shared_ptr<Chat::Data> object)
      : TreeItem(elementType, storingTypes, ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode)
      , dataObject_(object)
      , newItemsFlag_(false)
   {
   }

public:
   std::shared_ptr<Chat::Data> getDataObject() const {return dataObject_;}
   bool updateNewItemsFlag();
   bool getNewItemsFlag() const;
protected:
   std::shared_ptr<Chat::Data> dataObject_;
   bool newItemsFlag_;
};

#endif // TREEMAPCLASSES_H
