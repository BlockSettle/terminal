#ifndef __CHAT_TREE_NODE_TYPE_H__
#define __CHAT_TREE_NODE_TYPE_H__

#include <QObject>

namespace ChatUIDefinitions
{
Q_NAMESPACE

enum class ChatTreeNodeType : uint32_t
{
   //Root
   RootNode = 1,

   //RootNode accept types
   CategoryGroupNode = 2,

   //CategoryNode accept types (subcategory)
   SearchElement = 4,
   RoomsElement = 8,
   ContactsElement = 16,
   AllUsersElement = 32,
   OTCReceivedResponsesElement = 64,
   OTCSentResponsesElement = 128,

   //Subcategory accept types
   NoDataNode = 256,
   MessageDataNode = 512,
   OTCReceivedResponseNode = 1024,
   OTCSentResponseNode = 2048
};

Q_ENUM_NS(ChatUIDefinitions::ChatTreeNodeType)

}
#endif // __CHAT_TREE_NODE_TYPE_H__
