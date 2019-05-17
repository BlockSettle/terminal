#include "AcceptedNodeTypes.h"

AcceptedNodeTypes::AcceptedNodeTypes(const std::vector<ChatUIDefinitions::ChatTreeNodeType>& acceptedTypesList)
{
   dataTypeFlags_ = 0;

   for (const auto& it : acceptedTypesList) {
      if (it == ChatUIDefinitions::ChatTreeNodeType::NoDataNode) {
         // no childs will be accepted
         dataTypeFlags_ = 0;
         break;
      }

      dataTypeFlags_ |= static_cast<uint32_t>(it);
   }
}

bool AcceptedNodeTypes::IsNodeTypeAccepted(const ChatUIDefinitions::ChatTreeNodeType& dataType) const
{
   return (dataTypeFlags_ & static_cast<uint32_t>(dataType)) != 0;
}
