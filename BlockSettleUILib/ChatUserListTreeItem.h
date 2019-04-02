#ifndef CHATUSERLISTTREEITEM_H
#define CHATUSERLISTTREEITEM_H

#include <QList>
#include <QVariant>
#include <QVector>

#include "ChatUserData.h"
#include "ChatProtocol/DataObjects.h"

class ChatUserListTreeItem 
{

public:
   enum Category
   {
      NoneCategory = -1,
      RoomCategory,
      ContactCategory,
      CategoryCount
   };

   // Category construct
   explicit ChatUserListTreeItem(ChatUserListTreeItem *parent = 0);
   explicit ChatUserListTreeItem(Category category, ChatUserListTreeItem *parent = 0);
   explicit ChatUserListTreeItem(ChatUserDataPtr dataPtr, ChatUserListTreeItem *parent = 0);
   explicit ChatUserListTreeItem(Chat::RoomDataPtr dataPtr, ChatUserListTreeItem *parent = 0);

   ~ChatUserListTreeItem();

   Category category() const;
   ChatUserDataPtr userData() const;
   Chat::RoomDataPtr roomData() const;

   ChatUserListTreeItem *parent();
   ChatUserListTreeItem *child(int number);
   int childCount() const;

   void addCategoryAsChild(Category category);
   void addUserAsChild(ChatUserDataPtr user);
   void addRoomAsChild(Chat::RoomDataPtr room);
   void removeChildren();

   int childNumber() const;

private:
   ChatUserListTreeItem *parentItem_;
   QList<ChatUserListTreeItem *> childItems_;

   ChatUserDataPtr userDataPtr_;
   Chat::RoomDataPtr roomDataPtr_;
   Category category_;
};

#endif