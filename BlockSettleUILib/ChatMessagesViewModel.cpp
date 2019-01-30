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
   return static_cast<int>(Column::last);
}

int ChatMessagesViewModel::rowCount(const QModelIndex &parent) const
{
   return messages_[currentChatId_].size();
}

QVariant ChatMessagesViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }


   if (role == Qt::DisplayRole) {
	   //You should always check what role is requested
	   //beacause if it will be for example count role
	   //your code without checks could return 0 count
	   //and as result display role will have no effect
	   //and you will not see your headers
	   switch (static_cast<Column>(section)) {
	   case Column::Time:
		   return tr("Time");

	   case Column::User:
		   return tr("User");

	   case Column::Message:
		   return tr("Message");

	   default:
		   break;
	   }
   }
   return {};
}

QVariant ChatMessagesViewModel::data(const QModelIndex &index, int role) const
{
   const auto column = static_cast<Column>(index.column());
   if (role == Qt::TextColorRole) {
      switch (column) {
      case Column::User:
         return QColor(Qt::gray);
      default: break;
      }
   }
   else if (role == Qt::DisplayRole) {
      if (messages_[currentChatId_].empty()) {
          return QVariant();
      }

      switch (column) {
         case Column::Time:
         {
            const auto dateTime = messages_[currentChatId_][index.row()]->getDateTime().toLocalTime();

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
            QString sender = messages_[currentChatId_][index.row()]->getSenderId();
            if (sender == ownUserId_) {
               sender = ownSender;
            }
            return sender;
         }

         case Column::Message:
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

void ChatMessagesViewModel::onSingleMessageUpdate(const std::shared_ptr<Chat::MessageData> &msg)
{
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   beginInsertRows(QModelIndex(), rowIdx, rowIdx);
   messages_[currentChatId_].push_back(msg);
   endInsertRows();
}

void ChatMessagesViewModel::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages)
{
   for (const auto &msg : messages) {
      if ((msg->getSenderId() == currentChatId_) || (msg->getReceiverId() == currentChatId_)) {
         const int beginRow = messages_[currentChatId_].size();
         beginInsertRows(QModelIndex(), beginRow, beginRow);
         messages_[currentChatId_].push_back(msg);
         endInsertRows();
      }
      else {
         messages_[msg->getSenderId()].push_back(msg);
      }
      NotificationCenter::notify(bs::ui::NotifyType::NewChatMessage, { tr("New message") });
   }
}
