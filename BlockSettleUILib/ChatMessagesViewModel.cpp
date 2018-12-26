#include "ChatMessagesViewModel.h"

#include "ChatClient.h"

#include <QDebug>

#include "FastLock.h"


ChatMessagesViewModel::ChatMessagesViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{
}


int ChatMessagesViewModel::columnCount(const QModelIndex &parent) const
{
   return 2;
}


int ChatMessagesViewModel::rowCount(const QModelIndex &parent) const
{
   if (messages_[currentChatId_].empty())
   {
       return 0;
   }

   return messages_[currentChatId_].size();
}


QVariant ChatMessagesViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   switch (section)
   {
      case 0:
         return tr("Time");

      case 1:
         return tr("Message");

      default:
         break;
   }

   return QVariant();
}


QVariant ChatMessagesViewModel::data(const QModelIndex &index, int role) const
{
   if (role == Qt::DisplayRole)
   {
      if (messages_[currentChatId_].empty())
      {
          return QVariant();
      }

      switch (index.column())
      {
         case 0:
         {
            auto dateTime = messages_[currentChatId_][index.row()].first;

            if (dateTime.date() == QDate::currentDate())
            {
               return dateTime.time().toString(QString::fromUtf8("hh:mm"));
            }
            else
            {
               return dateTime.toString();
            }
         }

         case 1:
            return messages_[currentChatId_][index.row()].second;

         default:
            break;
      }
   }

   return QVariant();
}


void ChatMessagesViewModel::ensureChatId(const QString& chatId)
{
   if (!messages_.contains(chatId))
   {
      messages_.insert(chatId, MessagesHistory());
   }
}


void ChatMessagesViewModel::onSwitchToChat(const QString& chatId)
{
   beginResetModel();
   ensureChatId(chatId);
   currentChatId_ = chatId;
   endResetModel();
}


void ChatMessagesViewModel::onSingleMessageUpdate(const QDateTime& date, const QString& messageText)
{
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   beginInsertRows(QModelIndex(), rowIdx, rowIdx);
   messages_[currentChatId_].push_back(std::make_pair(date, prependMessage(messageText)));
   endInsertRows();
}


QString ChatMessagesViewModel::prependMessage(const QString& messageText, const QString& senderId)
{
   QString displayMessage = QStringLiteral("[")
         + ( senderId.isEmpty() ? currentChatId_ : senderId )
         + QStringLiteral("]: ") + messageText;
   return displayMessage;
}


void ChatMessagesViewModel::onMessagesUpdate(const std::vector<std::string>& messages)
{
   if (messages.size() == 0)
      return;

   ensureChatId(currentChatId_);

   int beginRow = messages_[currentChatId_].size();
   int endRow = messages_[currentChatId_].size() + messages.size() - 1;
   beginInsertRows(QModelIndex(), beginRow, endRow);

   std::for_each(messages.begin(), messages.end(), [&](const std::string& msgData) {

      auto receivedMessage = Chat::MessageData::fromJSON(msgData);
      auto senderId = receivedMessage->getSenderId();

      messages_[currentChatId_].push_back(
                  std::make_pair(receivedMessage->getDateTime()
                                , prependMessage(receivedMessage->getMessageData(), senderId)));
   });

   endInsertRows();
}
