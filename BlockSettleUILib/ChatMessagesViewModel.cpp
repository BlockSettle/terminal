#include "ChatMessagesViewModel.h"
#include "ChatClient.h"
#include "NotificationCenter.h"


ChatMessagesViewModel::ChatMessagesViewModel(QObject* parent)
   : QAbstractTableModel(parent)
{
}

int ChatMessagesViewModel::columnCount(const QModelIndex &parent) const
{
   return 3;
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
         return tr("User");
      
      case 2:
         return tr("Message");

      default:
         break;
   }
   return {};
}

QVariant ChatMessagesViewModel::data(const QModelIndex &index, int role) const
{
   if (index.column() == 0) {
      if(role == Qt::TextAlignmentRole)
         return Qt::AlignLeft;
   }
   if (index.column() == 1) {
      if(role == Qt::TextAlignmentRole)
         return Qt::AlignLeft;
      if(role == Qt::TextColorRole)
         return QVariant::fromValue(QColor(Qt::gray));
   }
   if (role == Qt::DisplayRole) {
      if (messages_[currentChatId_].empty()) {
          return QVariant();
      }

      switch (index.column()) {
         case 0:
         {
            auto dateTime = std::get<0>(messages_[currentChatId_][index.row()]);

            if (dateTime.date() == QDate::currentDate()) {
               return dateTime.time().toString(QString::fromUtf8("hh:mm"));
            }
            else {
               return dateTime.toString();
            }
         }

         case 1:
            return std::get<1>(messages_[currentChatId_][index.row()]);
         
         case 2:
            return std::get<2>(messages_[currentChatId_][index.row()]);

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
   messages_[currentChatId_].push_back(prependMessage(date, messageText));
   endInsertRows();
}

std::tuple<QDateTime, QString, QString> ChatMessagesViewModel::prependMessage(const QDateTime& date, const QString& messageText, const QString& senderId)
{
   static const auto ownSender = tr("you");
   QString sender = senderId.isEmpty() ? ownSender : senderId;
   if (sender == ownUserId_) {
      sender = ownSender;
   }
   return { date, QStringLiteral("[") + sender + QStringLiteral("]: "), messageText};
}
void ChatMessagesViewModel::onMessagesUpdate(const std::vector<std::string>& messages)
{
   if (messages.size() == 0)
      return;

   for (const auto &msg : messages) {
      const auto receivedMessage = Chat::MessageData::fromJSON(msg);
      const auto senderId = receivedMessage->getSenderId();
      const auto dateTime = receivedMessage->getDateTime();
      const auto msgTuple = prependMessage(dateTime, receivedMessage->getMessageData(), senderId);

      if (senderId == currentChatId_) {
         const int beginRow = messages_[currentChatId_].size();
         beginInsertRows(QModelIndex(), beginRow, beginRow);
         messages_[currentChatId_].push_back(msgTuple);
         endInsertRows();
      }
      else {
         messages_[senderId].push_back(msgTuple);
      }
      NotificationCenter::notify(bs::ui::NotifyType::NewMessage, { tr("New message") });
   }
}
