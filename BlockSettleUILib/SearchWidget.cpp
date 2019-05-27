#include "SearchWidget.h"
#include "ui_SearchWidget.h"

#include <QTimer>

constexpr int kShowEmptyFoundUserListTimeoutMs = 3000;

SearchWidget::SearchWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::SearchWidget)
   , listVisibleTimer_(new QTimer)
{
   ui_->setupUi(this);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textEdited,
           this, &SearchWidget::searchUserTextEdited);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textChanged,
           this, &SearchWidget::searchTextChanged);
}

SearchWidget::~SearchWidget()
{
}

void SearchWidget::init()
{
   ui_->searchResultTreeView->setGeometry(0, 0,
                                          ui_->chatSearchLineEdit->width(),
                                          ui_->chatSearchLineEdit->height() * 3);
   ui_->searchResultTreeView->setVisible(false);

   listVisibleTimer_->setSingleShot(true);
   connect(listVisibleTimer_.get(), &QTimer::timeout, [this] {
      setListVisible(false);
   });
}

bool SearchWidget::isLineEditEnabled() const
{
   return ui_->chatSearchLineEdit->isEnabled();
}

bool SearchWidget::isListVisible() const
{
   return ui_->searchResultTreeView->isVisible();
}

QString SearchWidget::searchText() const
{
   return ui_->chatSearchLineEdit->text();
}

void SearchWidget::clearLineEdit()
{
   ui_->chatSearchLineEdit->clear();
}

void SearchWidget::startListAutoHide()
{
   listVisibleTimer_->start(kShowEmptyFoundUserListTimeoutMs);
}

void SearchWidget::setLineEditEnabled(bool value)
{
   ui_->chatSearchLineEdit->setEnabled(value);
}

void SearchWidget::setListVisible(bool value)
{
   ui_->searchResultTreeView->setVisible(value);
   if (value) {
      ui_->chatUsersVerticalSpacer_->changeSize(20, 50 - ui_->chatSearchLineEdit->height() * 3);
   } else {
      ui_->chatUsersVerticalSpacer_->changeSize(20, 50);
   }
   layout()->update();
   listVisibleTimer_->stop();
}

void SearchWidget::setSearchText(QString value)
{
   ui_->chatSearchLineEdit->setText(value);
}
