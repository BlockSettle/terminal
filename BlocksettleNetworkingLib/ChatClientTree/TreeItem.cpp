#include "TreeItem.h"

#include <iostream>

ChatUIDefinitions::ChatTreeNodeType TreeItem::getType() const
{
   return ownType_;
}

TreeItem *TreeItem::getParent() const {
   return parent_;
}

QString TreeItem::getDisplayName() const
{
   return displayName_;
}

TreeItem::~TreeItem()
{
   deleteChildren();
}

bool TreeItem::insertItem(TreeItem *item)
{
   bool supported = isChildSupported(item);
   //assert(supported);
   if (supported) {
      addChild(item);
      return true;
   }
   return false;
}

int TreeItem::selfIndex() const
{
   if (parent_) {
       auto thizIt = std::find(parent_->children_.begin(), parent_->children_.end(), this);
       if (thizIt != parent_->children_.end()) {
          return static_cast<int>(std::distance(parent_->children_.begin(), thizIt));
       }
   }
   return 0;
}

int TreeItem::notEmptyChildrenCount()
{
   int count = 0;
   for (const auto & child : children_) {
       count += child->getChildren().size() > 0 ? 1 : 0;
   }
   return count;
}

void TreeItem::deleteChildren()
{
   for (auto child : children_) {
      delete child;
   }
   children_.clear();
}

void TreeItem::setParent(TreeItem *parent)
{
   parent_ = parent;
}

void TreeItem::addChild(TreeItem *item)
{
   if (item->getParent()) {
      item->getParent()->removeChild(item);
   }
   item->setParent(this);
   children_.push_back(item);
   onChildAdded(item);
}

void TreeItem::onChildAdded(TreeItem* item)
{}

void TreeItem::removeChild(TreeItem *item)
{
   children_.erase(std::remove(std::begin(children_), std::end(children_), item), std::end(children_));
}

TreeItem *TreeItem::findSupportChild(TreeItem *item)
{
   auto found = std::find_if(children_.begin(), children_.end(),
                                       [item](TreeItem* child) {
      return child->isChildSupported(item);
   });

   if (found != children_.end()) {
      return *found;
   }
   return nullptr;
}

TreeItem::TreeItem(ChatUIDefinitions::ChatTreeNodeType type
                  , const std::vector<ChatUIDefinitions::ChatTreeNodeType>& acceptedTypes
                  , ChatUIDefinitions::ChatTreeNodeType parentType
                  , const QString& displayName)
   : QObject(nullptr)
   , ownType_{type}
   , acceptNodeTypes_{acceptedTypes}
   , targetParentType_{parentType}
   , parent_{nullptr}
   , displayName_{displayName}
{
}

bool TreeItem::isChildSupported(const TreeItem *item) const
{
   return isChildTypeSupported(item->getType())
      && item->isParentSupported(this);
}

bool TreeItem::isChildTypeSupported(const ChatUIDefinitions::ChatTreeNodeType& childType) const
{
   return acceptNodeTypes_.IsNodeTypeAccepted(childType);
}

bool TreeItem::isParentSupported(const TreeItem* item) const
{
   return targetParentType_ == item->getType();
}

const TreeItem *TreeItem::recursiveRoot() const
{
   if (!parent_) {
      return this;
   }

   return parent_->recursiveRoot();
}
