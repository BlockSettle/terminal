
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

public slots:
   //! Activate view, set focus, select first item if not selected.
   void activate();

protected:
   void keyPressEvent(QKeyEvent *event) override;
}; // class TreeViewWithENterKey

#endif // _TREEVIEWWITHENTERKEY_H_INCLUDED_
