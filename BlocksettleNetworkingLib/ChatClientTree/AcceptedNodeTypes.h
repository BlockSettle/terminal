#ifndef __DATA_ACCEPT_TYPE_H__
#define __DATA_ACCEPT_TYPE_H__

#include <vector>

#include "ChatTreeNodeType.h"

class AcceptedNodeTypes
{
public:
   explicit AcceptedNodeTypes(const std::vector<ChatUIDefinitions::ChatTreeNodeType>& acceptedTypesList);
   ~AcceptedNodeTypes() noexcept = default;

   AcceptedNodeTypes(const AcceptedNodeTypes&) = delete;
   AcceptedNodeTypes& operator = (const AcceptedNodeTypes&) = delete;

   AcceptedNodeTypes(AcceptedNodeTypes&&) = delete;
   AcceptedNodeTypes& operator = (AcceptedNodeTypes&&) = delete;

   bool IsNodeTypeAccepted(const ChatUIDefinitions::ChatTreeNodeType& dataType) const;

private:
   uint32_t    dataTypeFlags_;
};

#endif // __DATA_ACCEPT_TYPE_H__
