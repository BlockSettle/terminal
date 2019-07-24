#include "SearchWidget.h"
#include "ui_SearchWidget.h"
#include "UserSearchModel.h"
#include "ChatSearchListViewItemStyle.h"
#include "ChatClient.h"
#include "chat.pb.h"
#include "UserHasher.h"

#include <QTimer>
#include <QMenu>
#include <memory>

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

void SearchWidget::init(std::shared_ptr<ChatClient> chatClient)
{
   chatClient_ = chatClient;
   ui_->chatSearchLineEdit->setActionsHandler(chatClient);
   userSearchModel_->setItemStyle(std::make_shared<ChatSearchListViewItemStyle>());
   ui_->searchResultTreeView->setModel(userSearchModel_.get());

   connect(chatClient_.get(), &ChatClient::SearchUserListReceived,
           this, &SearchWidget::onSearchUserListReceived);

   connect(userSearchModel_.get(), &QAbstractItemModel::rowsInserted,
           this, &SearchWidget::resetTreeView);
   connect(userSearchModel_.get(), &QAbstractItemModel::rowsRemoved,
           this, &SearchWidget::resetTreeView);
   connect(userSearchModel_.get(), &QAbstractItemModel::modelReset,
           this, &SearchWidget::resetTreeView);

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

void SearchWidget::onSearchUserListReceived(const std::vector<std::shared_ptr<Chat::Data> > &users, bool emailEntered)
{
   std::vector<UserSearchModel::UserInfo> userInfoList;
   const QString search = searchText();
   bool isEmail = kRxEmail.match(search).hasMatch();
   std::string hash = chatClient_->deriveKey(search.toStdString());
   for (const auto &user : users) {
      if (user && user->has_user()) {
         const std::string &userId = user->user().user_id();
         if (isEmail && userId != hash) {
            continue;
         }
         auto status = UserSearchModel::UserStatus::ContactUnknown;
         auto contact = chatClient_->getContact(userId);
         if (!contact.user_id().empty()) {
            auto contactStatus = contact.status();
            switch (contactStatus) {
            case Chat::CONTACT_STATUS_ACCEPTED:
               status = UserSearchModel::UserStatus::ContactAccepted;
               break;
            case Chat::CONTACT_STATUS_INCOMING:
               status = UserSearchModel::UserStatus::ContactPendingIncoming;
               break;
            case Chat::CONTACT_STATUS_OUTGOING_PENDING:
            case Chat::CONTACT_STATUS_OUTGOING:
               status = UserSearchModel::UserStatus::ContactPendingOutgoing;
               break;
            case Chat::CONTACT_STATUS_REJECTED:
               status = UserSearchModel::UserStatus::ContactRejected;
               break;
            default:
               assert(false);
               break;
            }
         }
         userInfoList.emplace_back(QString::fromStdString(userId), status);
      }
   }
   userSearchModel_->setUsers(userInfoList);

   bool visible = true;
   if (isEmail) {
      visible = emailEntered || !userInfoList.empty();
      if (visible) {
         clearSearchLineOnNextInput();
      }
   } else {
      visible = !userInfoList.empty();
   }
   setListVisible(visible);

   // hide popup after a few sec
   if (visible && userInfoList.empty()) {
      startListAutoHide();
   }
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
   const QString id = index.data(Qt::DisplayRole).toString();
   const auto status = index.data(UserSearchModel::UserStatusRole).value<UserSearchModel::UserStatus>();
   switch (status) {
   case UserSearchModel::UserStatus::ContactUnknown: {
      emit addFriendRequied(id);
      break;
   }
   case UserSearchModel::UserStatus::ContactAccepted:
   case UserSearchModel::UserStatus::ContactPendingIncoming:
   case UserSearchModel::UserStatus::ContactPendingOutgoing: {
      emit showUserRoom(id);
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

void SearchWidget::onSearchUserTextEdited()
{
   std::string userToAdd = searchText().toStdString();
   if (userToAdd.empty() || userToAdd.length() < 3) {
      setListVisible(false);
      userSearchModel_->setUsers({});
      return;
   }

   QRegularExpressionMatch match = kRxEmail.match(QString::fromStdString(userToAdd));
   if (match.hasMatch()) {
      userToAdd = chatClient_->deriveKey(userToAdd);
   } else if (UserHasher::KeyLength < userToAdd.length()) {
      return; //Initially max key is 12 symbols
   }
   chatClient_->sendSearchUsersRequest(userToAdd);
}
