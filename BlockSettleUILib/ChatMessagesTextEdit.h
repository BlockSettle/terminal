#ifndef __CHAT_MESSAGES_VIEW_MODEL_H__
#define __CHAT_MESSAGES_VIEW_MODEL_H__

#include <memory>
#include <QTextBrowser>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <tuple>
#include <QTextTable>
#include <QImage>

namespace Chat {
   class MessageData;
}

class ChatMessagesTextEdit : public QTextBrowser
{
   Q_OBJECT

public:
   ChatMessagesTextEdit(QWidget* parent = nullptr);
   ~ChatMessagesTextEdit() noexcept override = default;

public:
   void setOwnUserId(const std::string &userId) { ownUserId_ = QString::fromStdString(userId); }
   
signals:
   void MessageRead(const std::shared_ptr<Chat::MessageData> &) const;
   void	rowsInserted();

protected:
   enum class Column {
      Time,
      User,
      Status,
      Message,
      last
   };

   QString data(const int &row, const Column &column);
   QImage statusImage(const int &row);

   
public slots:
   void onSwitchToChat(const QString& chatId);
   void onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>> & messages, bool isFirstFetch);
   void onSingleMessageUpdate(const std::shared_ptr<Chat::MessageData> &);
   void onMessageIdUpdate(const QString& oldId, const QString& newId,const QString& chatId);
   void onMessageStatusChanged(const QString& messageId, const QString chatId, int newStatus);
   void	urlActivated(const QUrl &link);


private:
   using MessagesHistory = std::vector<std::shared_ptr<Chat::MessageData>>;
   QMap<QString, MessagesHistory> messages_;
   MessagesHistory messagesToLoadMore_;
   QString   currentChatId_;
   QString   ownUserId_;
   
private:
   std::shared_ptr<Chat::MessageData> findMessage(const QString& chatId, const QString& messageId);
   void notifyMessageChanged(std::shared_ptr<Chat::MessageData> message);
   void insertMessage(std::shared_ptr<Chat::MessageData> message);
   void insertLoadMore();
   void loadMore();
   QString toHtmlText(const QString &text);

   QTextTableFormat tableFormat;
   QTextTable *table;
};

#endif
