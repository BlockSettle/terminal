#include "ChatUserListTreeItem.h"

ChatUserListTreeItem::ChatUserListTreeItem(ChatUserListTreeItem *parent)
{
   parentItem_ = parent;
   category_ = NoneCategory;
}

ChatUserListTreeItem::ChatUserListTreeItem(Category category, ChatUserListTreeItem *parent)
{
   category_ = category;
   parentItem_ = parent;
}

ChatUserListTreeItem::ChatUserListTreeItem(ChatUserDataPtr dataPtr, ChatUserListTreeItem *parent)
{
   userDataPtr_ = dataPtr;
   parentItem_ = parent;
   category_ = NoneCategory;
}

ChatUserListTreeItem::ChatUserListTreeItem(Chat::RoomDataPtr dataPtr, ChatUserListTreeItem *parent)
{
   roomDataPtr_ = dataPtr;
   parentItem_ = parent;
   category_ = NoneCategory;
}

ChatUserListTreeItem::Category ChatUserListTreeItem::category() const
{
   return category_;
}
	
ChatUserDataPtr ChatUserListTreeItem::userData() const
{
   return userDataPtr_;
}
	
Chat::RoomDataPtr ChatUserListTreeItem::roomData() const
{
   return roomDataPtr_;
}

ChatUserListTreeItem *ChatUserListTreeItem::parent()
{
   return parentItem_;
}

ChatUserListTreeItem *ChatUserListTreeItem::child(int number)
{
   return childItems_.value(number);
}

ChatUserListTreeItem::~ChatUserListTreeItem()
{
   qDeleteAll(childItems_);
}

int ChatUserListTreeItem::childCount() const
{
   return childItems_.count();
}

void ChatUserListTreeItem::addCategoryAsChild(Category category)
{
   ChatUserListTreeItem *treeItem = new ChatUserListTreeItem(category, this);
   childItems_.push_back(treeItem);
}

void ChatUserListTreeItem::addUserAsChild(ChatUserDataPtr user)
{
   ChatUserListTreeItem *treeItem = new ChatUserListTreeItem(user, this);
   childItems_.push_back(treeItem);
}
    
void ChatUserListTreeItem::addRoomAsChild(Chat::RoomDataPtr room)
{
   ChatUserListTreeItem *treeItem = new ChatUserListTreeItem(room, this);
   childItems_.push_back(treeItem);
}

void ChatUserListTreeItem::removeChildren()
{
   qDeleteAll(childItems_);
   childItems_.clear();
}

int ChatUserListTreeItem::childNumber() const
{
   if (parentItem_)
      return parentItem_->childItems_.indexOf(const_cast<ChatUserListTreeItem*>(this));

   return 0;
}
