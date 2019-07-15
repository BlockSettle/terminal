#include "SearchWidget.h"
#include "ui_SearchWidget.h"
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
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textChanged,
           this, &SearchWidget::onInputTextChanged);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::keyDownPressed,
           this, &SearchWidget::focusResults);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::customContextMenuRequested,
           this, &SearchWidget::showContextMenu);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::activated,
           this, &SearchWidget::onItemClicked);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::clicked,
           this, &SearchWidget::onItemClicked);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::leaveRequired,
           this, &SearchWidget::leaveSearchResults);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::leaveWithCloseRequired,
           this, &SearchWidget::leaveAndCloseSearchResults);
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

void SearchWidget::init(std::shared_ptr<ChatSearchActionsHandler> handler)
{
   ui_->chatSearchLineEdit->setActionsHandler(handler);

   setMaximumHeight(kBottomSpace + kRowHeigth * kMaxVisibleRows +
                    ui_->chatSearchLineEdit->height() + kUserListPaddings + kFullHeightMargins);
   setMinimumHeight(kBottomSpace + ui_->chatSearchLineEdit->height());

   ui_->searchResultTreeView->setVisible(false);
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

void SearchWidget::clearSearchLineOnNextInput()
{
   ui_->chatSearchLineEdit->setResetOnNextInput(true);
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
   ui_->searchResultTreeView->setCurrentIndex(QModelIndex());
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
   onItemClicked(index);
}

void SearchWidget::focusResults()
{
   if (ui_->searchResultTreeView->isVisible()) {
      ui_->searchResultTreeView->setFocus();
      auto index = ui_->searchResultTreeView->model()->index(0, 0);
      ui_->searchResultTreeView->setCurrentIndex(index);
   }
}

void SearchWidget::onItemClicked(const QModelIndex &index)
{
   if (!index.isValid()) {
      return;
   }
   QString id = index.data(Qt::DisplayRole).toString();
   auto status = index.data(UserSearchModel::UserStatusRole).value<UserSearchModel::UserStatus>();
   switch (status) {
   case UserSearchModel::UserStatus::ContactUnknown: {
      emit addFriendRequied(id);
      break;
   }
   case UserSearchModel::UserStatus::ContactAccepted:
   case UserSearchModel::UserStatus::ContactPendingIncoming:
   case UserSearchModel::UserStatus::ContactPendingOutgoing: {
      emit removeFriendRequired(id);
      break;
   }
   default:
      return;
   }
}

void SearchWidget::leaveSearchResults()
{
   ui_->chatSearchLineEdit->setFocus();
   ui_->searchResultTreeView->clearSelection();
   auto currentIndex = ui_->searchResultTreeView->currentIndex();
   ui_->searchResultTreeView
         ->selectionModel()
         ->setCurrentIndex(currentIndex, QItemSelectionModel::Deselect);
}

void SearchWidget::leaveAndCloseSearchResults()
{
   ui_->chatSearchLineEdit->setFocus();
   setListVisible(false);
}

void SearchWidget::onInputTextChanged(const QString &text)
{
   if (text.isEmpty()) {
      setListVisible(false);
   }
}
