#include <QDebug>
#include <QDesktopServices>
#include "ChatMessagesTextEdit.h"
#include "ChatClient.h"
#include "ChatProtocol.h"
#include "NotificationCenter.h"

const int FIRST_FETCH_MESSAGES_SIZE = 20;

ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextBrowser(parent)
{
   tableFormat.setBorder(0);
   tableFormat.setCellPadding(0);
   tableFormat.setCellSpacing(0);

   QVector <QTextLength> col_widths;
   col_widths << QTextLength (QTextLength::FixedLength, 140);
   col_widths << QTextLength (QTextLength::FixedLength, 85);
   col_widths << QTextLength (QTextLength::FixedLength, 40);
   col_widths << QTextLength (QTextLength::VariableLength, 50);
   tableFormat.setColumnWidthConstraints (col_widths);

   setAlignment(Qt::AlignHCenter);
   setAutoFormatting(QTextEdit::AutoAll);
   setAcceptRichText(true);
   setOpenExternalLinks(false);
   setOpenLinks(false);

   connect(this, &QTextBrowser::anchorClicked, this, &ChatMessagesTextEdit::urlActivated);
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

         if (dateTime.date() == QDate::currentDate()) {
            return dateTime.time().toString(QString::fromUtf8("hh:mm:ss"));
         }
         else {
            return dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss"));
         }
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");
         QString sender = messages_[currentChatId_][row]->getSenderId();
         if (sender == ownUserId_) {
            sender = ownSender;
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
         return QString(QLatin1String("[%1] %2")).arg(messages_[currentChatId_][row]->getId(), messages_[currentChatId_][row]->getMessageData());

      default:
         break;
   }
   
   return QString();
}

QImage ChatMessagesTextEdit::statusImage(const int &row) {

   std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][row];
   if (message->getSenderId() != ownUserId_){
      return QImage();
   }
   int state = message->getState();
   QImage statusImage = QImage(QLatin1Literal(":/ICON_STATUS_OFFLINE"), "PNG");
   
   if (state & static_cast<int>(Chat::MessageData::State::Sent)){
      statusImage = QImage(QLatin1Literal(":/ICON_STATUS_CONNECTING"), "PNG");
   }
   
   if (state & static_cast<int>(Chat::MessageData::State::Acknowledged)){
      statusImage = QImage(QLatin1Literal(":/ICON_STATUS_ONLINE"), "PNG");
   }
   
   if (state & static_cast<int>(Chat::MessageData::State::Read)){
      statusImage = QImage(QLatin1Literal(":/ICON_STATUS_READ"), "PNG");
   }
   
   return statusImage;
}

void ChatMessagesTextEdit::onSwitchToChat(const QString& chatId)
{
   qDebug() << "onSwitchToChat";
   currentChatId_ = chatId;
   messages_.clear();
   messagesToLoadMore_.clear();
   
   clear();
   table = NULL;
}

void  ChatMessagesTextEdit::urlActivated(const QUrl &link) {
   if (link.toString() == QLatin1Literal("load_more")) {
      loadMore();
   } else {
      QDesktopServices::openUrl(link);
   }
}

void ChatMessagesTextEdit::insertMessage(std::shared_ptr<Chat::MessageData> msg) {
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   messages_[currentChatId_].push_back(msg);
   
   /* add text */
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::End);
   table = cursor.insertTable(1, 4, tableFormat);

   QString time = data(rowIdx, Column::Time);
   table->cellAt(0, 0).firstCursorPosition().insertText(time);

   QString user = data(rowIdx, Column::User);
   table->cellAt(0, 1).firstCursorPosition().insertText(user);

   QImage image = statusImage(rowIdx);
   table->cellAt(0, 2).firstCursorPosition().insertImage(image);

   QString message = data(rowIdx, Column::Message);
   message = toHtmlText(message);
   table->cellAt(0, 3).firstCursorPosition().insertHtml(message);
}

void ChatMessagesTextEdit::insertLoadMore() {
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::Start);
   cursor.insertHtml(QLatin1Literal("<a href=\"load_more\" style=\"color:#20709a\">Load More...</a>"));
}

void ChatMessagesTextEdit::loadMore() {

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
      table->cellAt(0, 0).firstCursorPosition().insertText(time);

      QString user = data(i, Column::User);
      table->cellAt(0, 1).firstCursorPosition().insertText(user);

      QImage image = statusImage(i);
      table->cellAt(0, 2).firstCursorPosition().insertImage(image);

      QString message = data(i, Column::Message);
      message = toHtmlText(message);
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
      message->setFlag((Chat::MessageData::State)newStatus);
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
         table->cellAt(0, 0).firstCursorPosition().insertText(time);

         QString user = data(distance, Column::User);
         table->cellAt(0, 1).firstCursorPosition().insertText(user);

         QImage image = statusImage(distance);
         table->cellAt(0, 2).firstCursorPosition().insertImage(image);

         QString message = data(distance, Column::Message);
         message = toHtmlText(message);
         table->cellAt(0, 3).firstCursorPosition().insertHtml(message);

         emit rowsInserted();
      }
   }
}

void ChatMessagesTextEdit::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages, bool isFirstFetch)
{
   if (isFirstFetch) {
      for (const auto &msg : messages) {
         if ((msg->getSenderId() == currentChatId_) || (msg->getReceiverId() == currentChatId_)) {
            messagesToLoadMore_.push_back(msg);
         }
         else {
            messages_[msg->getSenderId()].push_back(msg);
         }

         NotificationCenter::notify(bs::ui::NotifyType::NewChatMessage, { tr("New message") });
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
         if ((msg->getSenderId() == currentChatId_) || (msg->getReceiverId() == currentChatId_)) {
            insertMessage(msg);
         }
         else {
            messages_[msg->getSenderId()].push_back(msg);
         }

         NotificationCenter::notify(bs::ui::NotifyType::NewChatMessage, { tr("New message") });
      }
   }

   emit rowsInserted();
}

QString ChatMessagesTextEdit::toHtmlText(const QString &text) {
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
      QString hyperlinkText = QLatin1Literal("<a href=\"") + linkText + QLatin1Literal("\" style=\"color:#20709a\">") + linkText + QLatin1Literal("</a>");

      changedText = changedText.replace(startIndex, endIndex - startIndex, hyperlinkText);

      index = startIndex + hyperlinkText.length();
   }

   // replace linefeed with <br>
   changedText.replace(QLatin1Literal("\n"), QLatin1Literal("<br>"));

   return changedText;
}
