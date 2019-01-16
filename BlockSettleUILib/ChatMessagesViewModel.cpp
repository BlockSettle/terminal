#include "ChatMessagesViewModel.h"
#include "ChatClient.h"
#include "ChatProtocol.h"
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
      case ChatMessageColumns::Time:
         return tr("Time");

      case ChatMessageColumns::User:
         return tr("User");
      
      case ChatMessageColumns::Message:
         return tr("Message");

      default:
         break;
   }
   return {};
}

QVariant ChatMessagesViewModel::data(const QModelIndex &index, int role) const
{
   if (index.column() == ChatMessageColumns::Time) {
      if(role == Qt::TextAlignmentRole)
         return Qt::AlignLeft;
   }
   if (index.column() == ChatMessageColumns::User) {
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
         case ChatMessageColumns::Time:
         {
            auto dateTime = messages_[currentChatId_][index.row()]->getDateTime();

            if (dateTime.date() == QDate::currentDate()) {
               return dateTime.time().toString(QString::fromUtf8("hh:mm"));
            }
            else {
               return dateTime.toString();
            }
         }

         case ChatMessageColumns::User:
            return messages_[currentChatId_][index.row()]->getSenderId();
         
         case ChatMessageColumns::Message:
            return messages_[currentChatId_][index.row()]->getMessageData();

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
   messages_.clear();
   endResetModel();
}

void ChatMessagesViewModel::onSingleMessageUpdate(const QDateTime& date, const QString& messageText)
{
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   beginInsertRows(QModelIndex(), rowIdx, rowIdx);
   messages_[currentChatId_].push_back(prependMessage(date, messageText));
   endInsertRows();
}

std::shared_ptr<Chat::MessageData> ChatMessagesViewModel::prependMessage(const QDateTime& date, const QString& messageText, const QString& senderId)
{
   static const auto ownSender = tr("you");
   QString sender = senderId.isEmpty() ? ownSender : senderId;
   if (sender == ownUserId_) {
      sender = ownSender;
   }
   
   return std::shared_ptr<Chat::MessageData>(
                        new Chat::MessageData(
                                    QStringLiteral("[") + sender + QStringLiteral("]: "), 
                                    QString::fromStdString(""), // receiver
                                    QString::fromStdString(""), // id
                                    date, 
                                    messageText)
                        );
}

void ChatMessagesViewModel::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages)
{
   for (const auto &msg : messages) {
      std::shared_ptr<Chat::MessageData> message_parts = prependMessage(msg->getDateTime(), msg->getMessageData(), msg->getSenderId());

      if ((msg->getSenderId() == currentChatId_) || (msg->getReceiverId() == currentChatId_)) {
         const int beginRow = messages_[currentChatId_].size();
         beginInsertRows(QModelIndex(), beginRow, beginRow);
         messages_[currentChatId_].push_back(message_parts);
         endInsertRows();
      }
      else {
         messages_[msg->getSenderId()].push_back(message_parts);
      }
      NotificationCenter::notify(bs::ui::NotifyType::NewChatMessage, { tr("New message") });
   }
}
