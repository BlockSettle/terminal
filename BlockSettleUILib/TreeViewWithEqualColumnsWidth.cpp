
#include "TreeViewWithEqualColumnsWidth.h"

#include <QHeaderView>


//
// TreeViewWithEqualColumnsWidth
//

TreeViewWithEqualColumnsWidth::TreeViewWithEqualColumnsWidth(QWidget *parent)
   : QTreeView(parent)
{
   header()->setStretchLastSection(true);
}

void TreeViewWithEqualColumnsWidth::resizeEvent(QResizeEvent *e)
{
   QTreeView::resizeEvent(e);

   const int c = header()->count() - header()->hiddenSectionCount();
   const int w = width() / c - c + 1;

   for (int i = 0; i < header()->count(); ++i) {
      if (!header()->isSectionHidden(i)) {
         header()->resizeSection(i, w);
      }
   }
}
