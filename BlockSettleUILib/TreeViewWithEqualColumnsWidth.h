
#ifndef TREEVIEWWITHEQUALCOLUMNSWIDTH_H_INCLUDED
#define TREEVIEWWITHEQUALCOLUMNSWIDTH_H_INCLUDED

#include <QTreeView>


//
// TreeViewWithEqualColumnsWidth
//

//! Tree view with equal columns widthes.
class TreeViewWithEqualColumnsWidth : public QTreeView
{
   Q_OBJECT

public:
   explicit TreeViewWithEqualColumnsWidth(QWidget *parent);
   ~TreeViewWithEqualColumnsWidth() noexcept override = default;

protected:
   void resizeEvent(QResizeEvent *e) override;
}; // class TreeViewWithEqualColumnsWidth

#endif // TREEVIEWWITHEQUALCOLUMNSWIDTH_H_INCLUDED
