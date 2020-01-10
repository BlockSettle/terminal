/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef _TREEVIEWWITHENTERKEY_H_INCLUDED_
#define _TREEVIEWWITHENTERKEY_H_INCLUDED_

#include <QTreeView>


//
// TreeViewWithEnterKey
//

//! Just a tree view that emits signal on "Enter" key pressing.
class TreeViewWithEnterKey : public QTreeView
{
   Q_OBJECT

signals:
   //! "Enter" key was pressed.
   void enterKeyPressed(const QModelIndex &);

public:
   explicit TreeViewWithEnterKey(QWidget *parent = nullptr);
   ~TreeViewWithEnterKey() noexcept override = default;

   QStyleOptionViewItem viewOptions() const override;

   // Could be used to disable deselection from mouse clicks in empty space.
   // Enabled by default.
   void setEnableDeselection(bool enableDeselection);

public slots:
   //! Activate view, set focus, select first item if not selected.
   void activate();

protected:
   void keyPressEvent(QKeyEvent *event) override;
   void mouseReleaseEvent(QMouseEvent *event) override;

   bool enableDeselection_{true};
}; // class TreeViewWithENterKey

#endif // _TREEVIEWWITHENTERKEY_H_INCLUDED_
