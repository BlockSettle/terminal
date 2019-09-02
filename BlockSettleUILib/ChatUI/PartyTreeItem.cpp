#include "PartyTreeItem.h"

PartyTreeItem::PartyTreeItem(const QVariant& data, UI::ElementType modelType, PartyTreeItem* parent /*= nullptr*/)
   : itemData_(data)
   , modelType_(modelType)
   , parentItem_(parent)
{
}

PartyTreeItem::~PartyTreeItem()
{
   qDeleteAll(childItems_);
}

PartyTreeItem* PartyTreeItem::child(int number)
{
   Q_ASSERT(number >= 0 && number < childItems_.size());
   return childItems_.value(number);
}

int PartyTreeItem::childCount() const
{
   return childItems_.count();
}

int PartyTreeItem::columnCount() const
{
   return 1;
}

QVariant PartyTreeItem::data() const
{
   return itemData_;
}

bool PartyTreeItem::insertChildren(PartyTreeItem* item)
{
   childItems_.push_back(item);
   return true;
}

PartyTreeItem* PartyTreeItem::parent()
{
   return parentItem_;
}

bool PartyTreeItem::removeChildren(int position, int count)
{
   if (position < 0 || position + count > childItems_.size()) {
      return false;
   }

   for (int row = 0; row < count; ++row)
      delete childItems_.takeAt(position);

   return true;
}

void PartyTreeItem::removeAll()
{
   qDeleteAll(childItems_);
   childItems_.clear();
}

int PartyTreeItem::childNumber() const
{
   if (parentItem_) {
      return parentItem_->childItems_.indexOf(const_cast<PartyTreeItem*>(this));
   }

   Q_ASSERT(false);
   return 0;
}

bool PartyTreeItem::setData(const QVariant& value)
{
   itemData_ = value;
   return true;
}

UI::ElementType PartyTreeItem::modelType() const
{
   return modelType_;
}

void PartyTreeItem::increaseUnreadedCounter(int newMessageCount)
{
   Q_ASSERT(newMessageCount > 0);
   unreadedCounter_ += newMessageCount;
}

void PartyTreeItem::decreaseUnreadedCounter(int seenMessageCount)
{
   unreadedCounter_ -= seenMessageCount;
   unreadedCounter_ = std::max(unreadedCounter_, 0);
}

bool PartyTreeItem::hasNewMessages() const
{
   return unreadedCounter_ > 0;
}
