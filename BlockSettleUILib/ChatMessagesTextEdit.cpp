#include <QDebug>
#include <QDesktopServices>
#include <set>
#include "ChatMessagesTextEdit.h"
#include "ChatClient.h"
#include "ChatProtocol/ChatProtocol.h"

const int FIRST_FETCH_MESSAGES_SIZE = 20;

ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextBrowser(parent), internalStyle_(this)
{
   tableFormat.setBorder(0);
   tableFormat.setCellPadding(0);
   tableFormat.setCellSpacing(0);

   QVector <QTextLength> col_widths;
   col_widths << QTextLength (QTextLength::FixedLength, 110);
   col_widths << QTextLength (QTextLength::FixedLength, 20);
   col_widths << QTextLength (QTextLength::FixedLength, 90);
   col_widths << QTextLength (QTextLength::VariableLength, 50);
   tableFormat.setColumnWidthConstraints (col_widths);

   setAlignment(Qt::AlignHCenter);
   setAutoFormatting(QTextEdit::AutoAll);
   setAcceptRichText(true);
   setOpenExternalLinks(false);
   setOpenLinks(false);

   statusImageOffline_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_OFFLINE"), "PNG");
   statusImageConnecting_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_CONNECTING"), "PNG");
   statusImageOnline_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_ONLINE"), "PNG");
   statusImageRead_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_READ"), "PNG");

   connect(this, &QTextBrowser::anchorClicked, this, &ChatMessagesTextEdit::urlActivated);

   userMenu_ = new QMenu(this);
   QAction *addUserToContactsAction = userMenu_->addAction(QObject::tr("Add to contacts"));
   addUserToContactsAction->setStatusTip(QObject::tr("Click to add user to contact list"));
   connect(addUserToContactsAction, &QAction::triggered,
      [this](bool) {
         emit sendFriendRequest(username_); 
      }
   );
}

QString ChatMessagesTextEdit::data(const int &row, const Column &column)
{
   if (messages_[currentChatId_].empty()) {
       return QString();
   }

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = messages_[currentChatId_][row]->getDateTime().toLocalTime();
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");
         QString sender = messages_[currentChatId_][row]->getSenderId();
         if (sender == ownUserId_) {
            sender = ownSender;
         } else if (isGroupRoom_) {
            sender = toHtmlUsername(sender);
         }
         return sender;
      }
      case Column::Status:{
         std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][row];
         if (message->getSenderId() != ownUserId_){
            if (!(message->getState() & static_cast<int>(Chat::MessageData::State::Read))){
               emit MessageRead(message);
            }
            return QString();
            
         }
         int state = message->getState();
         QString status = QLatin1String("Sending");

         if (state & static_cast<int>(Chat::MessageData::State::Sent)){
            status = QLatin1String("Sent");
         }
         
         if (state & static_cast<int>(Chat::MessageData::State::Acknowledged)){
            status = QLatin1String("Delivered");
         }
         
         if (state & static_cast<int>(Chat::MessageData::State::Read)){
            status = QLatin1String("Read");
         }
         return status;
      }

      case Column::Message:
         return toHtmlText(messages_[currentChatId_][row]->getMessageData());

      default:
         break;
   }
   
   return QString();
}

QImage ChatMessagesTextEdit::statusImage(const int &row)
{

   std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][row];
   if (message->getSenderId() != ownUserId_){
      return QImage();
   }
   int state = message->getState();

   QImage statusImage = statusImageOffline_;
   
   if (state & static_cast<int>(Chat::MessageData::State::Sent)){
      statusImage = statusImageConnecting_;
   }
   
   if (state & static_cast<int>(Chat::MessageData::State::Acknowledged)){
      statusImage = statusImageOnline_;
   }
   
   if (state & static_cast<int>(Chat::MessageData::State::Read)){
      statusImage = statusImageRead_;
   }
   
   return statusImage;
}

void ChatMessagesTextEdit::switchToChat(const QString& chatId, bool isGroupRoom)
{
   currentChatId_ = chatId;
   isGroupRoom_ = isGroupRoom;
   messages_.clear();
   messagesToLoadMore_.clear();
   
   clear();
   table = NULL;

   emit userHaveNewMessageChanged(chatId, false, false);
}

void  ChatMessagesTextEdit::urlActivated(const QUrl &link) {
   if (link.toString() == QLatin1Literal("load_more")) {
      loadMore();
   } 
   else if  (link.toString().startsWith(QLatin1Literal("user:"))) {
      username_ = link.toString().mid(QString(QLatin1Literal("user:")).length());
      userMenu_->exec(QCursor::pos());
   }
   else {
      QDesktopServices::openUrl(link);
   }
}

void ChatMessagesTextEdit::insertMessage(std::shared_ptr<Chat::MessageData> msg)
{
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   messages_[currentChatId_].push_back(msg);
   
   /* add text */
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::End);
   table = cursor.insertTable(1, 4, tableFormat);

   QString time = data(rowIdx, Column::Time);
   table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

   QImage image = statusImage(rowIdx);
   table->cellAt(0, 1).firstCursorPosition().insertImage(image);

   QString user = data(rowIdx, Column::User);
   table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

   QString message = data(rowIdx, Column::Message);
   table->cellAt(0, 3).firstCursorPosition().insertHtml(message);
}

void ChatMessagesTextEdit::insertLoadMore()
{
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::Start);
   cursor.insertHtml(QString(QLatin1Literal("<a href=\"load_more\" style=\"color:%1\">Load More...</a>")).arg(internalStyle_.colorHyperlink().name()));
}

void ChatMessagesTextEdit::loadMore()
{

   // delete insert more button
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::Start);
   cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor, 1);
   cursor.removeSelectedText();

   // load more messages
   int i = 0;
   for (const auto &msg: messagesToLoadMore_) {
      cursor.movePosition(QTextCursor::Start);
      cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, i * 2);
      
      messages_[currentChatId_].insert(messages_[currentChatId_].begin() + i, msg);

      table = cursor.insertTable(1, 4, tableFormat);

      QString time = data(i, Column::Time);
      table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

      QImage image = statusImage(i);
      table->cellAt(0, 1).firstCursorPosition().insertImage(image);

      QString user = data(i, Column::User);
      table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

      QString message = data(i, Column::Message);
      table->cellAt(0, 3).firstCursorPosition().insertHtml(message);

      i++;
   }

   messagesToLoadMore_.clear();
}

void ChatMessagesTextEdit::onSingleMessageUpdate(const std::shared_ptr<Chat::MessageData> &msg)
{
   insertMessage(msg);

   emit rowsInserted();
}

void ChatMessagesTextEdit::onMessageIdUpdate(const QString& oldId, const QString& newId, const QString& chatId)
{
   std::shared_ptr<Chat::MessageData> message = findMessage(chatId, oldId);
   
   if (message != nullptr){
      message->setId(newId);
      message->setFlag(Chat::MessageData::State::Sent);
      notifyMessageChanged(message);
   }
}

void ChatMessagesTextEdit::onMessageStatusChanged(const QString& messageId, const QString chatId, int newStatus)
{
   std::shared_ptr<Chat::MessageData> message = findMessage(chatId, messageId);
   
   if (message != nullptr){
      message->updateState(newStatus);
      notifyMessageChanged(message);
   }
}

std::shared_ptr<Chat::MessageData> ChatMessagesTextEdit::findMessage(const QString& chatId, const QString& messageId)
{
   std::shared_ptr<Chat::MessageData> found = nullptr;
   if (messages_.contains(chatId)) {
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [messageId](std::shared_ptr<Chat::MessageData> data){
         return data->getId() == messageId;
      });
      
      if (it != messages_[chatId].end()) {
         found = *it;
      }
   }
   return found;
}

void ChatMessagesTextEdit::notifyMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   const QString chatId = message->getSenderId() == ownUserId_
                          ? message->getReceiverId()
                          : message->getSenderId();
   
   if (messages_.contains(chatId)) {
      QString id = message->getId();
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [id](std::shared_ptr<Chat::MessageData> data){
         return data->getId() == id;
      });
      
      if (it != messages_[chatId].end()) {
         int distance = static_cast<int>(std::distance(messages_[chatId].begin(), it));
         
         QTextCursor cursor(textCursor());
         cursor.movePosition(QTextCursor::Start);
         cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, distance * 2 + (messagesToLoadMore_.size() > 0));
         cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 2);
         cursor.removeSelectedText();
         
         table = cursor.insertTable(1, 4, tableFormat);
         
         QString time = data(distance, Column::Time);
         table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

         QImage image = statusImage(distance);
         table->cellAt(0, 1).firstCursorPosition().insertImage(image);

         QString user = data(distance, Column::User);
         table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

         QString message = data(distance, Column::Message);
         table->cellAt(0, 3).firstCursorPosition().insertHtml(message);

         emit rowsInserted();
      }
   }
}

void ChatMessagesTextEdit::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages, bool isFirstFetch)
{
   if (isFirstFetch) {
      for (const auto &msg : messages) {
         if (msg->getSenderId() == currentChatId_) {
            messagesToLoadMore_.push_back(msg);
            emit MessageRead(msg);
         }
         else {
            messages_[msg->getSenderId()].push_back(msg);
         }
      }

      if (messagesToLoadMore_.size() > FIRST_FETCH_MESSAGES_SIZE) { 
         /* display certain count of messages and thus remove the displayed messages from the messagesToLoadMore */

         // add "load more" hyperlink text
         insertLoadMore();
         
         // display last messages
         unsigned long i = 0;
         for (const auto &msg: messagesToLoadMore_) {
            if (i >= messagesToLoadMore_.size() - FIRST_FETCH_MESSAGES_SIZE) {
               insertMessage(msg);
            }

            i++;
         }

         // remove the messages shown
         for (i = 0; i < FIRST_FETCH_MESSAGES_SIZE; i++) {
            messagesToLoadMore_.pop_back();
         }
      } else { // flush all messages
         for (const auto &msg: messagesToLoadMore_) {
            insertMessage(msg);
         }

         messagesToLoadMore_.clear();
      }
   }
   else {
      for (const auto &msg : messages) {
         if (msg->getSenderId() == currentChatId_) {
            insertMessage(msg);
            if (!(msg->getState() & static_cast<int>(Chat::MessageData::State::Read))){
               emit MessageRead(msg);
            }
            
            emit userHaveNewMessageChanged(msg->getSenderId(), false, true);
         }
         else {
            messages_[msg->getSenderId()].push_back(msg);

            emit userHaveNewMessageChanged(msg->getSenderId(), true, false);
         }
      }
   }

   emit rowsInserted();
}

void ChatMessagesTextEdit::onRoomMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages, bool isFirstFetch)
{
   if (isFirstFetch) {
      for (const auto &msg : messages) {
         if (msg->getReceiverId() == currentChatId_) {
            messagesToLoadMore_.push_back(msg);
         }
         else {
            messages_[msg->getReceiverId()].push_back(msg);
         }
      }

      if (messagesToLoadMore_.size() > FIRST_FETCH_MESSAGES_SIZE) { 
         /* display certain count of messages and thus remove the displayed messages from the messagesToLoadMore */

         // add "load more" hyperlink text
         insertLoadMore();
         
         // display last messages
         unsigned long i = 0;
         for (const auto &msg: messagesToLoadMore_) {
            if (i >= messagesToLoadMore_.size() - FIRST_FETCH_MESSAGES_SIZE) {
               insertMessage(msg);
            }

            i++;
         }

         // remove the messages shown
         for (i = 0; i < FIRST_FETCH_MESSAGES_SIZE; i++) {
            messagesToLoadMore_.pop_back();
         }
      } else { // flush all messages
         for (const auto &msg: messagesToLoadMore_) {
            insertMessage(msg);
         }

         messagesToLoadMore_.clear();
      }
   }
   else {
      std::set<QString> receivers;
      for (const auto &msg : messages) {
         receivers.insert(msg->getReceiverId());
         if (msg->getReceiverId() == currentChatId_) {
            insertMessage(msg);
         }
         else {
            messages_[msg->getReceiverId()].push_back(msg);
         }
      }
      for (const QString& recv : receivers) {
         emit userHaveNewMessageChanged(recv, recv != currentChatId_, recv == currentChatId_);
      }
   }

   emit rowsInserted();
}

QString ChatMessagesTextEdit::toHtmlUsername(const QString &username)
{
   QString changedUsername = QString(QLatin1Literal("<a href=\"user:%1\" style=\"color:%2\">%1</a>")).arg(username).arg(internalStyle_.colorHyperlink().name());
   return changedUsername;
}

QString ChatMessagesTextEdit::toHtmlText(const QString &text)
{
   QString changedText = text;

   // make linkable
   int index = 0;
   int startIndex;

   while ((startIndex = changedText.indexOf(QLatin1Literal("https://"), index, Qt::CaseInsensitive)) != -1
      || (startIndex = changedText.indexOf(QLatin1Literal("http://"), index, Qt::CaseInsensitive)) != -1) {
      
      int endIndex = changedText.indexOf(QLatin1Literal(" "), startIndex);
      if (endIndex == -1) {
         endIndex = changedText.indexOf(QLatin1Literal("\n"), startIndex);
      }
      if (endIndex == -1) {
         endIndex = changedText.length();
      }

      QString linkText = changedText.mid(startIndex, endIndex - startIndex);
      QString hyperlinkText = QString(QLatin1Literal("<a href=\"%1\" style=\"color:%2\">%1</a>")).arg(linkText).arg(internalStyle_.colorHyperlink().name());

      changedText = changedText.replace(startIndex, endIndex - startIndex, hyperlinkText);

      index = startIndex + hyperlinkText.length();
   }

   // replace linefeed with <br>
   changedText.replace(QLatin1Literal("\n"), QLatin1Literal("<br>"));

   // set text color as white
   changedText = QString(QLatin1Literal("<font color=\"%1\">%2</font>")).arg(internalStyle_.colorWhite().name()).arg(changedText);

   return changedText;
}
