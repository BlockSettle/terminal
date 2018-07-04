#ifndef __WALLETS_TREE_VIEW_H__
#define __WALLETS_TREE_VIEW_H__

#include "TreeViewWithEnterKey.h"

class WalletsTreeView : public TreeViewWithEnterKey
{
    Q_OBJECT
public:
    explicit WalletsTreeView(QWidget *parent = nullptr);
    ~WalletsTreeView() override = default;

private:
    void resizeColumns();
};

#endif // __WALLETS_TREE_VIEW_H__
