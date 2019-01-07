#include "ChatMessagesViewModel.h"
#include "ChatClient.h"


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
   return messages_[currentChatId_].size();
}

QVariant ChatMessagesViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   switch (section) {
      case 0:
         return tr("Time");

      case 1:
         return tr("Message");

      default:
         break;
   }
   return {};
}

QVariant ChatMessagesViewModel::data(const QModelIndex &index, int role) const
{
   if (role == Qt::DisplayRole) {
      if (messages_[currentChatId_].empty()) {
          return QVariant();
      }

      switch (index.column()) {
         case 0:
         {
            auto dateTime = messages_[currentChatId_][index.row()].first;

            if (dateTime.date() == QDate::currentDate()) {
               return dateTime.time().toString(QString::fromUtf8("hh:mm"));
            }
            else {
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

void ChatMessagesViewModel::onSwitchToChat(const QString& chatId)
{
   beginResetModel();
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
   static const auto ownSender = tr("you");
   QString sender = senderId.isEmpty() ? ownSender : senderId;
   if (sender == ownUserId_) {
      sender = ownSender;
   }
   QString displayMessage = QStringLiteral("[") + sender + QStringLiteral("]: ") + messageText;
   return displayMessage;
}

void ChatMessagesViewModel::onMessagesUpdate(const std::vector<std::string>& messages)
{
   if (messages.size() == 0)
      return;

   for (const auto &msg : messages) {
      const auto receivedMessage = Chat::MessageData::fromJSON(msg);
      const auto senderId = receivedMessage->getSenderId();
      const auto dateTime = receivedMessage->getDateTime();
      const auto msgText = prependMessage(receivedMessage->getMessageData(), senderId);

      if (senderId == currentChatId_) {
         const int beginRow = messages_[currentChatId_].size();
         beginInsertRows(QModelIndex(), beginRow, beginRow);
         messages_[currentChatId_].push_back({dateTime, msgText });
         endInsertRows();
      }
      else {
         messages_[senderId].push_back({ dateTime, msgText });
      }
   }
}
