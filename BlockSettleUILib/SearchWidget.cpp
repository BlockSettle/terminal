#include "SearchWidget.h"
#include "ui_SearchWidget.h"

SearchWidget::SearchWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::SearchWidget)
{
   ui_->setupUi(this);
}

SearchWidget::~SearchWidget()
{
}
