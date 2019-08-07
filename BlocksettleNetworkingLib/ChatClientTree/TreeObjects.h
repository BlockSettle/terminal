#ifndef TREEOBJECTS_H
#define TREEOBJECTS_H

#include "TreeMapClasses.h"

class ChatRoomElement : public CategoryElement {
public:
   ChatRoomElement(const std::shared_ptr<Chat::Data> &data)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::RoomsElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::MessageDataNode}, data)
   {
      assert(data->has_room());
   }

   Chat::Data_Room* getRoomData() const;

   bool isChildSupported(const TreeItem *item) const override;
};

class ChatContactElement : public CategoryElement {
public:
   enum class OnlineStatus
   {
      Online,
      Offline
   };

   Q_ENUM(OnlineStatus)

   ChatContactElement(ChatUIDefinitions::ChatTreeNodeType contactNodeType, const std::shared_ptr<Chat::Data> &data)
      : CategoryElement(contactNodeType,
                        std::vector<ChatUIDefinitions::ChatTreeNodeType>{
                        ChatUIDefinitions::ChatTreeNodeType::MessageDataNode},
                        data)
   {
      assert(data->has_contact_record());
   }

   Chat::Data_ContactRecord *getContactData() const;

   OnlineStatus getOnlineStatus() const;
   void setOnlineStatus(const OnlineStatus &onlineStatus);

   bool isChildSupported(const TreeItem *item) const override;

protected:
   OnlineStatus onlineStatus_;
};

class ChatContactCompleteElement : public ChatContactElement {
public:
   ChatContactCompleteElement(std::shared_ptr<Chat::Data> data)
   : ChatContactElement(ChatUIDefinitions::ChatTreeNodeType::ContactsElement, data)
   {
      assert(data->has_contact_record());
   }
};

class ChatContactRequestElement : public ChatContactElement {
public:
   ChatContactRequestElement(std::shared_ptr<Chat::Data> data)
   : ChatContactElement(ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement, data)
   {
      assert(data->has_contact_record());
   }
};

class ChatSearchElement : public CategoryElement {
public:
   ChatSearchElement(std::shared_ptr<Chat::Data> data)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::SearchElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::NoDataNode}, data)
   {
      assert(data->has_user());
   }

   Chat::Data_User *getUserData() const;
};

class ChatUserElement : public CategoryElement
{
public:
   ChatUserElement(std::shared_ptr<Chat::Data> data)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::AllUsersElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::MessageDataNode}, data)
   {
      assert(data->has_user());
   }

   Chat::Data_User *getUserData() const;
};

class DisplayableDataNode : public TreeItem {
public:
   DisplayableDataNode(ChatUIDefinitions::ChatTreeNodeType messageParent,
                       ChatUIDefinitions::ChatTreeNodeType dataNodeType,
                       std::shared_ptr<Chat::Data> data)
      : TreeItem(dataNodeType, std::vector<ChatUIDefinitions::ChatTreeNodeType>{
                 ChatUIDefinitions::ChatTreeNodeType::NoDataNode}, messageParent)
      , data_(data)
   {}

   std::shared_ptr<Chat::Data> getDataObject() const;

protected:
   std::shared_ptr<Chat::Data> data_;
};

class TreeMessageNode : public DisplayableDataNode {
public:
   TreeMessageNode(ChatUIDefinitions::ChatTreeNodeType messageParent, std::shared_ptr<Chat::Data> data)
      : DisplayableDataNode(messageParent,
                            ChatUIDefinitions::ChatTreeNodeType::MessageDataNode, data)
   {
      assert(data->has_message());
   }

   std::shared_ptr<Chat::Data> getMessage() const {
      if (!data_->has_message()) {
         return nullptr;
      }
      return data_;
   }
};

/*
class ChatUserMessageNode : public TreeItem {
public:
   ChatUserMessageNode(std::shared_ptr<Chat::Data_Message> message)
      : TreeItem(NodeType::MessageDataNode, NodeType::NoDataNode, ChatUIDefinitions::ChatTreeNodeType::ContactsElement)
      , message_(message)
   {

   }
   std::shared_ptr<Chat::Data_Message> getMessage() const {return message_;}
private:
   std::shared_ptr<Chat::Data_Message> message_;
};*/

#endif // TREENODESIMPL_H
