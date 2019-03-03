#include "ChatMessagesTextEdit.h"
#include "ChatClient.h"
#include "ChatProtocol.h"
#include "NotificationCenter.h"


ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextEdit(parent)
{
   tableFormat.setBorder(0);
   tableFormat.setCellPadding(0);
   tableFormat.setCellSpacing(0);

   QVector <QTextLength> col_widths;
   col_widths << QTextLength (QTextLength::FixedLength, 140);
   col_widths << QTextLength (QTextLength::FixedLength, 70);
   col_widths << QTextLength (QTextLength::FixedLength, 16);
   col_widths << QTextLength (QTextLength::FixedLength, 55);
   col_widths << QTextLength (QTextLength::VariableLength, 50);
   tableFormat.setColumnWidthConstraints (col_widths);
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
            return dateTime.time().toString(QString::fromUtf8("hh:mm"));
         }
         else {
            return dateTime.toString();
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
      statusImage = QImage(QLatin1Literal(":/ICON_DOT"), "PNG");
   }
   
   return statusImage;
}

void ChatMessagesTextEdit::onSwitchToChat(const QString& chatId)
{
   currentChatId_ = chatId;
   messages_.clear();
   
   clear();
   table = NULL;
}

void ChatMessagesTextEdit::insertMessage(std::shared_ptr<Chat::MessageData> msg) {
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   messages_[currentChatId_].push_back(msg);
   
   /* add text */
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::End);
   table = cursor.insertTable(1, 5, tableFormat);

   QString time = data(rowIdx, Column::Time);
   table->cellAt(0, 0).firstCursorPosition().insertText(time);

   QString user = data(rowIdx, Column::User);
   table->cellAt(0, 1).firstCursorPosition().insertText(user);

   QImage image = statusImage(rowIdx);
   table->cellAt(0, 2).firstCursorPosition().insertImage(image);

   QString status = data(rowIdx, Column::Status);
   table->cellAt(0, 3).firstCursorPosition().insertText(status);

   QString message = data(rowIdx, Column::Message);
   table->cellAt(0, 4).firstCursorPosition().insertText(message);
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
         cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, distance * 2);
         cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 2);
         cursor.removeSelectedText();
         
         table = cursor.insertTable(1, 5, tableFormat);
         
         QString time = data(distance, Column::Time);
         table->cellAt(0, 0).firstCursorPosition().insertText(time);

         QString user = data(distance, Column::User);
         table->cellAt(0, 1).firstCursorPosition().insertText(user);

         QImage image = statusImage(distance);
         table->cellAt(0, 2).firstCursorPosition().insertImage(image);

         QString status = data(distance, Column::Status);
         table->cellAt(0, 3).firstCursorPosition().insertText(status);

         QString message = data(distance, Column::Message);
         table->cellAt(0, 4).firstCursorPosition().insertText(message);

         emit rowsInserted();
      }
   }
}

void ChatMessagesTextEdit::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages)
{
   for (const auto &msg : messages) {
      if ((msg->getSenderId() == currentChatId_) || (msg->getReceiverId() == currentChatId_)) {
         insertMessage(msg);
      }
      else {
         messages_[msg->getSenderId()].push_back(msg);
      }

      NotificationCenter::notify(bs::ui::NotifyType::NewChatMessage, { tr("New message") });
   }

   emit rowsInserted();
}
