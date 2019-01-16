#ifndef __CHAT_MESSAGES_VIEW_MODEL_H__
#define __CHAT_MESSAGES_VIEW_MODEL_H__


#include <memory>
#include <QAbstractTableModel>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <tuple>

namespace Chat {
   class MessageData;
}

class ChatMessagesViewModel : public QAbstractTableModel
{
   Q_OBJECT

public:
   ChatMessagesViewModel(QObject* parent = nullptr);
   ~ChatMessagesViewModel() noexcept override = default;

   ChatMessagesViewModel(const ChatMessagesViewModel&) = delete;
   ChatMessagesViewModel& operator = (const ChatMessagesViewModel&) = delete;
   ChatMessagesViewModel(ChatMessagesViewModel&&) = delete;
   ChatMessagesViewModel& operator = (ChatMessagesViewModel&&) = delete;

public:
   void setOwnUserId(const std::string &userId) { ownUserId_ = QString::fromStdString(userId); }

protected:
   enum class Column {
      Time,
      User,
      Message,
      last
   };

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
   void onSwitchToChat(const QString& chatId);
   void onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>> &);
   void onSingleMessageUpdate(const std::shared_ptr<Chat::MessageData> &);

private:
   using MessagesHistory = std::vector<std::shared_ptr<Chat::MessageData>>;
   QMap<QString, MessagesHistory> messages_;
   QString   currentChatId_;
   QString   ownUserId_;
};

#endif
