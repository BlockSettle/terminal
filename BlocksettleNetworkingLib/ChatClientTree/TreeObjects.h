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

   ChatContactElement(ChatUIDefinitions::ChatTreeNodeType contactNodeType, std::shared_ptr<Chat::ContactRecordData> data)
      : CategoryElement(contactNodeType,
                        std::vector<ChatUIDefinitions::ChatTreeNodeType>{
                        ChatUIDefinitions::ChatTreeNodeType::MessageDataNode},
                        data)
   {}

   std::shared_ptr<Chat::ContactRecordData> getContactData() const;

   OnlineStatus getOnlineStatus() const;
   void setOnlineStatus(const OnlineStatus &onlineStatus);

   bool isChildSupported(const TreeItem *item) const override;

protected:
   OnlineStatus onlineStatus_;
};

class ChatContactCompleteElement : public ChatContactElement {
public:
   ChatContactCompleteElement(std::shared_ptr<Chat::ContactRecordData> data)
   : ChatContactElement(ChatUIDefinitions::ChatTreeNodeType::ContactsElement, data)
   {

   }

   bool OTCTradingStarted() const;
   bool isOTCRequestor() const;
   bool haveUpdates() const;
   bool haveResponse() const;

   std::shared_ptr<Chat::OTCRequestData>  getOTCRequest() const;
   std::shared_ptr<Chat::OTCResponseData> getOTCResponse() const;
   std::shared_ptr<Chat::OTCUpdateData>   getLastOTCUpdate() const;

   void cleanupTrading();

protected:
   void onChildAdded(TreeItem* item) override;
private:
   void processOTCMessage(const std::shared_ptr<Chat::MessageData>& messageData);
protected:
   std::shared_ptr<Chat::OTCRequestData>  otcRequest_ = nullptr;
   std::shared_ptr<Chat::OTCResponseData> otcResponse_ = nullptr;
   std::shared_ptr<Chat::OTCUpdateData>       lastUpdate_ = nullptr;
};

class ChatContactRequestElement : public ChatContactElement {
public:
   ChatContactRequestElement(std::shared_ptr<Chat::ContactRecordData> data)
   : ChatContactElement(ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement, data)
   {

   }
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

class DisplayableDataNode : public TreeItem {
public:
   DisplayableDataNode(ChatUIDefinitions::ChatTreeNodeType messageParent,
                       ChatUIDefinitions::ChatTreeNodeType dataNodeType,
                       std::shared_ptr<Chat::DataObject> data)
      : TreeItem(dataNodeType, std::vector<ChatUIDefinitions::ChatTreeNodeType>{
                 ChatUIDefinitions::ChatTreeNodeType::NoDataNode}, messageParent)
      , data_(data)
   {}

   std::shared_ptr<Chat::DataObject> getDataObject() const;

protected:
   std::shared_ptr<Chat::DataObject> data_;
};

class TreeMessageNode : public DisplayableDataNode {
public:
   TreeMessageNode(ChatUIDefinitions::ChatTreeNodeType messageParent, std::shared_ptr<Chat::MessageData> message)
      : DisplayableDataNode(messageParent,
                            ChatUIDefinitions::ChatTreeNodeType::MessageDataNode, message)
   {}

   std::shared_ptr<Chat::MessageData> getMessage() const {
      return std::dynamic_pointer_cast<Chat::MessageData>(data_);
   }
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
