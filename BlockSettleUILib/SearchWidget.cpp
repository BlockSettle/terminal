#include "SearchWidget.h"
#include "ui_SearchWidget.h"
#include "ChatProtocol/DataObjects/UserData.h"
#include "UserSearchModel.h"

#include <QTimer>
#include <QMenu>
#include <QDebug>

constexpr int kShowEmptyFoundUserListTimeoutMs = 3000;
constexpr int kRowHeigth = 25;
constexpr int kUserListPaddings = 12;
constexpr int kMaxVisibleRows = 3;
constexpr int kBottomSpace = 20;

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
   connect(ui_->searchResultTreeView, &QTreeView::customContextMenuRequested,
           this, &SearchWidget::showContextMenu);
}

SearchWidget::~SearchWidget()
{
}

bool SearchWidget::eventFilter(QObject *watched, QEvent *event)
{
   if (ui_->searchResultTreeView->isVisible() && event->type() == QEvent::MouseButtonRelease) {
      QPoint pos = ui_->searchResultTreeView->mapFromGlobal(QCursor::pos());

      if (!ui_->searchResultTreeView->rect().contains(pos)) {
         setListVisible(false);
      }
   }

   return QWidget::eventFilter(watched, event);
}

void SearchWidget::init()
{
   setFixedHeight(kBottomSpace + kRowHeigth * kMaxVisibleRows +
                  ui_->chatSearchLineEdit->height() + kUserListPaddings + 6);

   ui_->searchResultTreeView->setHeaderHidden(true);
   ui_->searchResultTreeView->setRootIsDecorated(false);
   ui_->searchResultTreeView->setVisible(false);
   ui_->searchResultTreeView->setSelectionMode(QAbstractItemView::NoSelection);
   ui_->searchResultTreeView->setContextMenuPolicy(Qt::CustomContextMenu);

   ui_->notFoundLabel->setVisible(false);

   qApp->installEventFilter(this);

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

void SearchWidget::setSearchModel(const std::shared_ptr<QAbstractItemModel> &model)
{
   ui_->searchResultTreeView->setModel(model.get());
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
   bool hasUsers = ui_->searchResultTreeView->model()->rowCount() > 0;
   ui_->searchResultTreeView->setVisible(value && hasUsers);
   ui_->searchResultTreeView->scrollToTop();
   ui_->notFoundLabel->setVisible(value && !hasUsers);
   layout()->update();
   listVisibleTimer_->stop();
}

void SearchWidget::setSearchText(QString value)
{
   ui_->chatSearchLineEdit->setText(value);
}

void SearchWidget::showContextMenu(const QPoint &pos)
{
   QScopedPointer<QMenu, QScopedPointerDeleteLater> menu(new QMenu());
   int rowCount = ui_->searchResultTreeView->model()->rowCount();
   int visibleRows = rowCount >= kMaxVisibleRows ? kMaxVisibleRows : rowCount;
   ui_->searchResultTreeView->setFixedHeight(kRowHeigth * visibleRows + kUserListPaddings);
   auto index = ui_->searchResultTreeView->indexAt(pos);
   if (!index.isValid()) {
      return;
   }
   bool isInContacts = index.data(UserSearchModel::IsInContacts).toBool();
   QString id = index.data(Qt::DisplayRole).toString();
   qDebug() << "user:" << id << isInContacts;
   if (isInContacts) {
      auto action = menu->addAction(tr("Remove from contacts"), [this, id] {
         emit removeFriendRequired(id);
      });
      action->setStatusTip(tr("Click to remove user from contact list"));
   } else {
      auto action = menu->addAction(tr("Add to contacts"), [this, id] {
         emit removeFriendRequired(id);
      });
      action->setStatusTip(tr("Click to add user to contact list"));
   }
   menu->exec(ui_->searchResultTreeView->mapToGlobal(pos));
}
