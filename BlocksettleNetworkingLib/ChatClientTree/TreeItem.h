#ifndef TREEITEM_H
#define TREEITEM_H

#include "AcceptedNodeTypes.h"

#include <algorithm>
#include <list>
#include <vector>

#include <QObject>

class TreeItem : public QObject
{
Q_OBJECT

   friend class RootItem;

protected:
   TreeItem(ChatUIDefinitions::ChatTreeNodeType ownType
      , const std::vector<ChatUIDefinitions::ChatTreeNodeType>& acceptedTypes
      , ChatUIDefinitions::ChatTreeNodeType expectedParentType
      , const QString& displayName = QLatin1String("XXX_not_set"));

public:
   virtual ~TreeItem();

   ChatUIDefinitions::ChatTreeNodeType getType() const;
   TreeItem* getParent() const;

   virtual QString getDisplayName() const;

   virtual bool insertItem(TreeItem* item);
   int selfIndex() const;
   std::vector<TreeItem*> getChildren() { return children_; }
   int notEmptyChildrenCount();

   virtual bool isChildSupported(const TreeItem* item) const;
   bool isChildTypeSupported(const ChatUIDefinitions::ChatTreeNodeType& childType) const;
   bool isParentSupported(const TreeItem* item) const;

signals:
   void itemChanged(TreeItem*);
protected:
   const TreeItem *recursiveRoot() const;
   void addChild(TreeItem* item);
   void deleteChildren();
   void setParent(TreeItem* parent);
   void removeChild(TreeItem* item);

   TreeItem* findSupportChild(TreeItem* item);

protected:
   std::vector<TreeItem*> children_;
private:
   ChatUIDefinitions::ChatTreeNodeType  ownType_;
   AcceptedNodeTypes acceptNodeTypes_;
   ChatUIDefinitions::ChatTreeNodeType  targetParentType_;

   TreeItem*         parent_;
   QString           displayName_;
};



#endif // TREEITEM_H
