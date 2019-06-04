#include "SearchWidget.h"
#include "ui_SearchWidget.h"
#include "ChatProtocol/DataObjects/UserData.h"
#include "UserSearchModel.h"

#include <QTimer>
#include <QMenu>

constexpr int kShowEmptyFoundUserListTimeoutMs = 3000;
constexpr int kRowHeigth = 20;
constexpr int kUserListPaddings = 6;
constexpr int kMaxVisibleRows = 3;
constexpr int kBottomSpace = 25;
constexpr int kFullHeightMargins = 10;

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
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::keyDownPressed,
           this, &SearchWidget::focusResults);
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
   setMaximumHeight(kBottomSpace + kRowHeigth * kMaxVisibleRows +
                    ui_->chatSearchLineEdit->height() + kUserListPaddings + kFullHeightMargins);
   setMinimumHeight(kBottomSpace + ui_->chatSearchLineEdit->height());

   ui_->searchResultTreeView->setHeaderHidden(true);
   ui_->searchResultTreeView->setRootIsDecorated(false);
   ui_->searchResultTreeView->setVisible(false);
   ui_->searchResultTreeView->setSelectionMode(QAbstractItemView::SingleSelection);
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
   if (ui_->searchResultTreeView->model()) {
      disconnect(ui_->searchResultTreeView->model(), &QAbstractItemModel::rowsInserted,
                 this, &SearchWidget::resetTreeView);
      disconnect(ui_->searchResultTreeView->model(), &QAbstractItemModel::rowsRemoved,
                 this, &SearchWidget::resetTreeView);
      disconnect(ui_->searchResultTreeView->model(), &QAbstractItemModel::modelReset,
                 this, &SearchWidget::resetTreeView);
   }
   ui_->searchResultTreeView->setModel(model.get());
   connect(ui_->searchResultTreeView->model(), &QAbstractItemModel::rowsInserted,
           this, &SearchWidget::resetTreeView);
   connect(ui_->searchResultTreeView->model(), &QAbstractItemModel::rowsRemoved,
           this, &SearchWidget::resetTreeView);
   connect(ui_->searchResultTreeView->model(), &QAbstractItemModel::modelReset,
           this, &SearchWidget::resetTreeView);
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

void SearchWidget::resetTreeView()
{
   int rowCount = ui_->searchResultTreeView->model()->rowCount();
   int visibleRows = rowCount >= kMaxVisibleRows ? kMaxVisibleRows : rowCount;
   ui_->searchResultTreeView->setFixedHeight(kRowHeigth * visibleRows + kUserListPaddings);
}

void SearchWidget::showContextMenu(const QPoint &pos)
{
   QScopedPointer<QMenu, QScopedPointerDeleteLater> menu(new QMenu());
   auto index = ui_->searchResultTreeView->indexAt(pos);
   if (!index.isValid()) {
      return;
   }
   bool isInContacts = index.data(UserSearchModel::IsInContacts).toBool();
   QString id = index.data(Qt::DisplayRole).toString();
   if (isInContacts) {
      auto action = menu->addAction(tr("Remove from contacts"), [this, id] {
         emit removeFriendRequired(id);
      });
      action->setStatusTip(tr("Click to remove user from contact list"));
   } else {
      auto action = menu->addAction(tr("Add to contacts"), [this, id] {
         emit addFriendRequied(id);
      });
      action->setStatusTip(tr("Click to add user to contact list"));
   }
   menu->exec(ui_->searchResultTreeView->mapToGlobal(pos));
}

void SearchWidget::focusResults()
{
   if (ui_->searchResultTreeView->isVisible()) {
      ui_->searchResultTreeView->setFocus();
      auto index = ui_->searchResultTreeView->model()->index(0, 0);
      ui_->searchResultTreeView->setCurrentIndex(index);
   }
}
