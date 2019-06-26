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
   ContactsRequestElement = 32,
   AllUsersElement = 64,

   //Subcategory accept types
   NoDataNode = 128,
   MessageDataNode = 256
};

Q_ENUM_NS(ChatUIDefinitions::ChatTreeNodeType)

}
#endif // __CHAT_TREE_NODE_TYPE_H__
