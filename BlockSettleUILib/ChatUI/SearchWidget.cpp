#include <QTimer>
#include <QMenu>
#include <QKeyEvent>
#include <QUuid>

#include "SearchWidget.h"
#include "ui_SearchWidget.h"
#include "UserSearchModel.h"
#include "ChatUI/ChatSearchListViewItemStyle.h"

#include "chat.pb.h"

namespace  {
   constexpr int kShowEmptyFoundUserListTimeoutMs = 3000;
   constexpr int kRowHeigth = 20;
   constexpr int kUserListPaddings = 6;
   constexpr int kMaxVisibleRows = 3;
   constexpr int kBottomSpace = 25;
   constexpr int kFullHeightMargins = 10;

   const QRegularExpression kRxEmail(QStringLiteral(R"(^[a-z0-9._-]+@([a-z0-9-]+\.)+[a-z]+$)"),
      QRegularExpression::CaseInsensitiveOption);
}

SearchWidget::SearchWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::SearchWidget)
   , listVisibleTimer_(new QTimer)
   , userSearchModel_(new UserSearchModel)
{
   ui_->setupUi(this);

   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textEdited,
           this, &SearchWidget::onSearchUserTextEdited);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textChanged,
           this, &SearchWidget::searchTextChanged);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textChanged,
           this, &SearchWidget::onInputTextChanged);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::keyDownPressed,
           this, &SearchWidget::onFocusResults);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::keyEnterPressed,
           this, &SearchWidget::onFocusResults);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::keyEscapePressed,
           this, &SearchWidget::onCloseResult);

   connect(ui_->searchResultTreeView, &ChatSearchListVew::customContextMenuRequested,
           this, &SearchWidget::onShowContextMenu);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::activated,
           this, &SearchWidget::onItemClicked);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::clicked,
           this, &SearchWidget::onItemClicked);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::leaveRequired,
           this, &SearchWidget::onLeaveSearchResults);
   connect(ui_->searchResultTreeView, &ChatSearchListVew::leaveWithCloseRequired,
           this, &SearchWidget::onLeaveAndCloseSearchResults);
}

SearchWidget::~SearchWidget()
{
}

bool SearchWidget::eventFilter(QObject *watched, QEvent *event)
{
   if (ui_->searchResultTreeView->isVisible() && event->type() == QEvent::MouseButtonRelease) {
      QPoint pos = ui_->searchResultTreeView->mapFromGlobal(QCursor::pos());

      if (!ui_->searchResultTreeView->rect().contains(pos)) {
         onSetListVisible(false);
      }
   }

   return QWidget::eventFilter(watched, event);
}

void SearchWidget::init(const Chat::ChatClientServicePtr& chatClientServicePtr)
{
   chatClientServicePtr_ = chatClientServicePtr;
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::searchUserReply, this, &SearchWidget::onSearchUserReply);

   //chatClient_ = chatClient;
   //ui_->chatSearchLineEdit->setActionsHandler(chatClient);
   userSearchModel_->setItemStyle(std::make_shared<ChatSearchListViewItemStyle>());
   ui_->searchResultTreeView->setModel(userSearchModel_.get());

   //connect(chatClient_.get(), &ChatClient::SearchUserListReceived, this, &SearchWidget::onSearchUserListReceived);

   connect(userSearchModel_.get(), &QAbstractItemModel::rowsInserted,
           this, &SearchWidget::onResetTreeView);
   connect(userSearchModel_.get(), &QAbstractItemModel::rowsRemoved,
           this, &SearchWidget::onResetTreeView);
   connect(userSearchModel_.get(), &QAbstractItemModel::modelReset,
           this, &SearchWidget::onResetTreeView);

   setMaximumHeight(kBottomSpace + kRowHeigth * kMaxVisibleRows +
                    ui_->chatSearchLineEdit->height() + kUserListPaddings + kFullHeightMargins);
   setMinimumHeight(kBottomSpace + ui_->chatSearchLineEdit->height());

   ui_->searchResultTreeView->setVisible(false);
   ui_->notFoundLabel->setVisible(false);

   qApp->installEventFilter(this);

   listVisibleTimer_->setSingleShot(true);
   connect(listVisibleTimer_.get(), &QTimer::timeout, [this] {
      onSetListVisible(false);
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

void SearchWidget::onClearLineEdit()
{
   ui_->chatSearchLineEdit->clear();
}

void SearchWidget::onStartListAutoHide()
{
   listVisibleTimer_->start(kShowEmptyFoundUserListTimeoutMs);
}

void SearchWidget::onSetLineEditEnabled(bool value)
{
   ui_->chatSearchLineEdit->setEnabled(value);
}

void SearchWidget::onSetListVisible(bool value)
{
   bool hasUsers = ui_->searchResultTreeView->model()->rowCount() > 0;
   ui_->searchResultTreeView->setVisible(value && hasUsers);
   ui_->searchResultTreeView->scrollToTop();
   ui_->searchResultTreeView->setCurrentIndex(QModelIndex());
   ui_->notFoundLabel->setVisible(value && !hasUsers);
   layout()->update();
   listVisibleTimer_->stop();

   // hide popup after a few sec
   if (value && !hasUsers) {
      onStartListAutoHide();
   }
}

void SearchWidget::onSetSearchText(QString value)
{
   ui_->chatSearchLineEdit->setText(value);
}

void SearchWidget::onResetTreeView()
{
   int rowCount = ui_->searchResultTreeView->model()->rowCount();
   int visibleRows = rowCount >= kMaxVisibleRows ? kMaxVisibleRows : rowCount;
   ui_->searchResultTreeView->setFixedHeight(kRowHeigth * visibleRows + kUserListPaddings);
}

void SearchWidget::onShowContextMenu(const QPoint &pos)
{
   QScopedPointer<QMenu, QScopedPointerDeleteLater> menu(new QMenu());
   auto index = ui_->searchResultTreeView->indexAt(pos);
   if (!index.isValid()) {
      return;
   }

   onItemClicked(index);
}

void SearchWidget::onFocusResults()
{
   if (ui_->searchResultTreeView->isVisible()) {
      ui_->searchResultTreeView->setFocus();
      auto index = ui_->searchResultTreeView->model()->index(0, 0);
      ui_->searchResultTreeView->setCurrentIndex(index);
      return;
   }

   onSetListVisible(true);
}

void SearchWidget::onCloseResult()
{
   onSetListVisible(false);
}

void SearchWidget::onItemClicked(const QModelIndex &index)
{
   if (!index.isValid()) {
      return;
   }

   const QString id = index.data(Qt::DisplayRole).toString();
   const auto status = index.data(UserSearchModel::UserStatusRole).value<UserSearchModel::UserStatus>();
   switch (status) 
   {
      case UserSearchModel::UserStatus::ContactUnknown:
      {
         emit contactFriendRequest(id);
      }
      break;
      case UserSearchModel::UserStatus::ContactAccepted:
      case UserSearchModel::UserStatus::ContactPendingIncoming:
      case UserSearchModel::UserStatus::ContactPendingOutgoing:
      {
         emit showUserRoom(id);
      }
      break;
   default:
      return;
   }

   onSetListVisible(false);
   onSetSearchText({});
}

void SearchWidget::onLeaveSearchResults()
{
   ui_->chatSearchLineEdit->setFocus();
   ui_->searchResultTreeView->clearSelection();
   auto currentIndex = ui_->searchResultTreeView->currentIndex();
   ui_->searchResultTreeView->selectionModel()->setCurrentIndex(currentIndex, QItemSelectionModel::Deselect);
}

void SearchWidget::onLeaveAndCloseSearchResults()
{
   ui_->chatSearchLineEdit->setFocus();
   onSetListVisible(false);
}

void SearchWidget::onInputTextChanged(const QString &text)
{
   if (text.isEmpty()) {
      onSetListVisible(false);
   }
}

void SearchWidget::onSearchUserTextEdited()
{
   onSetListVisible(false);
   std::string userToAdd = searchText().toStdString();

   if (userToAdd.empty() || userToAdd.length() < 3) {
      onSetListVisible(false);
      userSearchModel_->setUsers({});
      return;
   }

   // ! Feature: Think how to prevent spamming server
   QUuid uid = QUuid::createUuid();
   lastSearchId_ = uid.toString(QUuid::WithoutBraces).toStdString();
   chatClientServicePtr_->SearchUser(userToAdd, lastSearchId_);
}

void SearchWidget::onSearchUserReply(const Chat::SearchUserReplyList& userHashList, const std::string& searchId)
{
   if (searchId != lastSearchId_) {
      return;
   }

   Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   std::vector<UserSearchModel::UserInfo> userInfoList;

   for (const auto& userHash : userHashList) {
      Chat::PrivatePartyState privatePartyState = clientPartyModelPtr->deducePrivatePartyStateForUser(userHash);

      auto status = UserSearchModel::UserStatus::ContactUnknown;
      switch (privatePartyState)
      {
      case Chat::PrivatePartyState::RequestedOutgoing:
         status = UserSearchModel::UserStatus::ContactPendingOutgoing;
         break;
      case Chat::PrivatePartyState::RequestedIncoming:
         status = UserSearchModel::UserStatus::ContactPendingIncoming;
         break;
      case Chat::PrivatePartyState::Rejected:
         status = UserSearchModel::UserStatus::ContactRejected;
         break;
      case Chat::PrivatePartyState::Initialized:
         status = UserSearchModel::UserStatus::ContactAccepted;
         break;
      default:
         break;
      }

      userInfoList.emplace_back(QString::fromStdString(userHash), status);
   }

   userSearchModel_->setUsers(userInfoList);

   bool visible = !userInfoList.empty();
   onSetListVisible(visible);

   // hide popup after a few sec
   if (visible && userInfoList.empty()) {
      onStartListAutoHide();
   }
}
