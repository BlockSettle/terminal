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
         
      case Column::Status:
         return tr("Status");

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
         case Column::Status:{
            std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][index.row()];
            if (message->getSenderId() != ownUserId_){
               if (!(message->getState() & static_cast<int>(Chat::MessageData::State::Read))){
                  emit MessageRead(message);
               }
               return QVariant();
               
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
            return QString(QLatin1String("[%1] %2")).arg(messages_[currentChatId_][index.row()]->getId(), messages_[currentChatId_][index.row()]->getMessageData());

         default:
            break;
      }
   } else if (role == Qt::DecorationRole) {
      switch (column) {
      case Column::Status:{
         std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][index.row()];
         if (message->getSenderId() != ownUserId_){
            return QVariant();
         }
         int state = message->getState();
         QIcon status(QLatin1Literal(":/ICON_STATUS_OFFLINE"));
         
         if (state & static_cast<int>(Chat::MessageData::State::Sent)){
            status = QIcon(QLatin1Literal(":/ICON_STATUS_CONNECTING"));
         }
         
         if (state & static_cast<int>(Chat::MessageData::State::Acknowledged)){
            status = QIcon(QLatin1Literal(":/ICON_STATUS_ONLINE"));
         }
         
         if (state & static_cast<int>(Chat::MessageData::State::Read)){
            status = QIcon(QLatin1Literal(":/ICON_DOT"));
         }
         
         return status;
      }
      default: break;
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

void ChatMessagesViewModel::onMessageIdUpdate(const QString& oldId, const QString& newId, const QString& chatId)
{
   std::shared_ptr<Chat::MessageData> message = findMessage(chatId, oldId);
   
   if (message != nullptr){
      message->setId(newId);
      message->setFlag(Chat::MessageData::State::Sent);
      notifyMessageChanged(message);
   }
}

void ChatMessagesViewModel::onMessageStatusChanged(const QString& messageId, const QString chatId, int newStatus)
{
   std::shared_ptr<Chat::MessageData> message = findMessage(chatId, messageId);
   
   if (message != nullptr){
      message->setFlag((Chat::MessageData::State)newStatus);
      notifyMessageChanged(message);
   }
}

std::shared_ptr<Chat::MessageData> ChatMessagesViewModel::findMessage(const QString& chatId, const QString& messageId)
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

void ChatMessagesViewModel::notifyMessageChanged(std::shared_ptr<Chat::MessageData> message)
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
         emit dataChanged(index(distance, static_cast<int>(Column::Status)), index(distance, static_cast<int>(Column::Message)), { Qt::DisplayRole });
      }
   }
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
