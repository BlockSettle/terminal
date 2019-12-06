/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
