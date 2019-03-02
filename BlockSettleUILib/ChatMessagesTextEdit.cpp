#include "ChatMessagesTextEdit.h"
#include "ChatClient.h"
#include "ChatProtocol.h"
#include "NotificationCenter.h"


ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextEdit(parent)
{
   tableFormat.setBorder(0);
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
   if (table == NULL) {
      QTextCursor cursor(textCursor());
      cursor.movePosition(QTextCursor::End);
      table = cursor.insertTable(1, 5, tableFormat);
   } else {
      table->appendRows(1);
   }

   QString time = QStringLiteral(" ") + data(rowIdx, Column::Time) + QStringLiteral(" ");
   table->cellAt(rowIdx, 0).firstCursorPosition().insertText(time);

   QString user = QStringLiteral(" ") + data(rowIdx, Column::User) + QStringLiteral(" ");
   table->cellAt(rowIdx, 1).firstCursorPosition().insertText(user);

   QImage image = statusImage(rowIdx);
   table->cellAt(rowIdx, 2).firstCursorPosition().insertImage(image);

   QString status = QStringLiteral(" ") + data(rowIdx, Column::Status) + QStringLiteral(" ");
   table->cellAt(rowIdx, 3).firstCursorPosition().insertText(status);

   QString message = QStringLiteral(" ") + data(rowIdx, Column::Message) + QStringLiteral(" ");
   table->cellAt(rowIdx, 4).firstCursorPosition().insertText(message);
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
         
         table->removeRows(distance, 1);
         table->insertRows(distance, 1);
         
         QString time = QStringLiteral(" ") + data(distance, Column::Time) + QStringLiteral(" ");
         table->cellAt(distance, 0).firstCursorPosition().insertText(time);

         QString user = QStringLiteral(" ") + data(distance, Column::User) + QStringLiteral(" ");
         table->cellAt(distance, 1).firstCursorPosition().insertText(user);

         QImage image = statusImage(distance);
         table->cellAt(distance, 2).firstCursorPosition().insertImage(image);

         QString status = QStringLiteral(" ") + data(distance, Column::Status) + QStringLiteral(" ");
         table->cellAt(distance, 3).firstCursorPosition().insertText(status);

         QString message = QStringLiteral(" ") + data(distance, Column::Message) + QStringLiteral(" ");
         table->cellAt(distance, 4).firstCursorPosition().insertText(message);

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
