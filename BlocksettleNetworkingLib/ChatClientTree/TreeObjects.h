#ifndef TREEOBJECTS_H
#define TREEOBJECTS_H

#include "TreeMapClasses.h"

class ChatRoomElement : public CategoryElement {
public:
   ChatRoomElement(std::shared_ptr<Chat::RoomData> data)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::RoomsElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::MessageDataNode}, data)
   {}

   std::shared_ptr<Chat::RoomData> getRoomData() const;

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

   ChatContactElement(std::shared_ptr<Chat::ContactRecordData> data)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::ContactsElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::MessageDataNode}, data)
   {}

   std::shared_ptr<Chat::ContactRecordData> getContactData() const;

   // TreeItem interface
   OnlineStatus getOnlineStatus() const;
   void setOnlineStatus(const OnlineStatus &onlineStatus);

   bool isChildSupported(const TreeItem *item) const override;

protected:
   OnlineStatus onlineStatus_;
};

class ChatSearchElement : public CategoryElement {
public:
   ChatSearchElement(std::shared_ptr<Chat::UserData> data)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::SearchElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::NoDataNode}, data)
   {}

   std::shared_ptr<Chat::UserData> getUserData() const;
};

class ChatUserElement : public CategoryElement
{
public:
   ChatUserElement(std::shared_ptr<Chat::UserData> data)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::AllUsersElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::MessageDataNode}, data)
   {}

   std::shared_ptr<Chat::UserData> getUserData() const;
};

class TreeMessageNode : public TreeItem {
public:
   TreeMessageNode(ChatUIDefinitions::ChatTreeNodeType messageParent, std::shared_ptr<Chat::MessageData> message)
      : TreeItem(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::NoDataNode}, messageParent)
      , message_(message)
   {}

   std::shared_ptr<Chat::MessageData> getMessage() const {return message_;}
private:
   std::shared_ptr<Chat::MessageData> message_;
};

class OTCSentResponseElement : public CategoryElement
{
public:
   OTCSentResponseElement(const std::string& otcId)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::OTCSentResponsesElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::OTCSentResponseNode}, nullptr)
   {}

   ~OTCSentResponseElement() override = default;
};

class OTCReceivedResponseElement : public CategoryElement
{
public:
   OTCReceivedResponseElement(const std::string& otcId)
      : CategoryElement(ChatUIDefinitions::ChatTreeNodeType::OTCReceivedResponsesElement, std::vector<ChatUIDefinitions::ChatTreeNodeType>{ChatUIDefinitions::ChatTreeNodeType::OTCReceivedResponseNode}, nullptr)
   {}

   ~OTCReceivedResponseElement() override = default;
};

/*
class ChatUserMessageNode : public TreeItem {
public:
   ChatUserMessageNode(std::shared_ptr<Chat::MessageData> message)
      : TreeItem(NodeType::MessageDataNode, NodeType::NoDataNode, ChatUIDefinitions::ChatTreeNodeType::ContactsElement)
      , message_(message)
   {

   }
   std::shared_ptr<Chat::MessageData> getMessage() const {return message_;}
private:
   std::shared_ptr<Chat::MessageData> message_;
};*/

#endif // TREENODESIMPL_H
