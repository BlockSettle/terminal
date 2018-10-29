#include "ExplorerWidget.h"
#include <QStringListModel>
#include "ui_ExplorerWidget.h"
#include "UiUtils.h"

ExplorerWidget::ExplorerWidget(QWidget *parent) :
   TabWithShortcut(parent)
   , ui_(new Ui::ExplorerWidget())
{
   ui_->setupUi(this);

   connect(ui_->searchBox, &QLineEdit::returnPressed, this, &ExplorerWidget::onSearchStarted);
}

ExplorerWidget::~ExplorerWidget() = default;

void ExplorerWidget::shortcutActivated(ShortcutType s)
{
   switch (s) {

   default:
      break;
   }
}

void ExplorerWidget::onSearchStarted()
{
   ui_->stackedWidget->count();
   int index = ui_->stackedWidget->currentIndex();
   if (index < ui_->stackedWidget->count() - 1)	{
      ui_->stackedWidget->setCurrentIndex(++index);
   }
   else {
      ui_->stackedWidget->setCurrentIndex(0);
   }
}
