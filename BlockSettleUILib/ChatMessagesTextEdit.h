#ifndef __CHAT_MESSAGES_VIEW_MODEL_H__
#define __CHAT_MESSAGES_VIEW_MODEL_H__

#include "ChatClient.h"
#include "ChatClientUserView.h"

#include <QDateTime>
#include <QImage>
#include <QMap>
#include <QMenu>
#include <QTextBrowser>
#include <QTextTable>
#include <QVector>

#include <memory>
#include <tuple>

namespace Chat {
   class MessageData;
}

class ChatMessagesTextEditStyle : public QWidget
{
   Q_OBJECT

   Q_PROPERTY(QColor color_hyperlink READ colorHyperlink
              WRITE setColorHyperlink)
   Q_PROPERTY(QColor color_white READ colorWhite
              WRITE setColorWhite)

public:
   inline explicit ChatMessagesTextEditStyle(QWidget *parent)
      : QWidget(parent), colorHyperlink_(Qt::blue), colorWhite_(Qt::white)
   {
      setVisible(false);
   }

   QColor colorHyperlink() const { return colorHyperlink_; }
   void setColorHyperlink(const QColor &colorHyperlink) {
      colorHyperlink_ = colorHyperlink;
   }

   QColor colorWhite() const { return colorWhite_; }
   void setColorWhite(const QColor &colorWhite) {
      colorWhite_ = colorWhite;
   }

private:
   QColor colorHyperlink_;
   QColor colorWhite_;
};

class ChatMessagesTextEdit : public QTextBrowser, public ViewItemWatcher
{
   Q_OBJECT

public:
   ChatMessagesTextEdit(QWidget* parent = nullptr);
   ~ChatMessagesTextEdit() noexcept override = default;

public:
   void setOwnUserId(const std::string &userId) { ownUserId_ = userId; }
   void switchToChat(const std::string& chatId, bool isGroupRoom = false);
   void setHandler(ChatItemActionsHandler* handler);
   void setMessageReadHandler(std::shared_ptr<ChatMessageReadHandler> handler);
   void setClient(std::shared_ptr<ChatClient> client);
   void setColumnsWidth(const int &time, const int &icon, const int &user, const int &message);
   void setIsChatTab(const bool &isChatTab);
   QString getFormattedTextFromSelection();

signals:
   void MessageRead(const std::shared_ptr<Chat::Data> &) const;
   void rowsInserted();
   void userHaveNewMessageChanged(const std::string &userId, const bool &haveNewMessage, const bool &isInCurrentChat);
   void sendFriendRequest(const std::string &userID);
   void addContactRequired(const QString &userId);

protected:
   enum class Column {
      Time,
      Status,
      User,
      Message,
      last
   };

   QString data(int row, const Column &column);
   QString dataMessage(int row, const Column &column);
   QImage statusImage(int row);

   void contextMenuEvent(QContextMenuEvent *e) override;

public slots:
   void onMessagesUpdate(const std::vector<std::shared_ptr<Chat::Data> > &messages, bool isFirstFetch);
   void onRoomMessagesUpdate(const std::vector<std::shared_ptr<Chat::Data>> & messages, bool isFirstFetch);
   void onSingleMessageUpdate(const std::shared_ptr<Chat::Data> &);
   void onMessageIdUpdate(const std::string& oldId, const std::string& newId,const std::string& chatId);
   void onMessageStatusChanged(const std::string& messageId, const std::string &chatId, int newStatus);
   void urlActivated(const QUrl &link);

private slots:
   void copyActionTriggered();
   void copyLinkLocationActionTriggered();
   void selectAllActionTriggered();
   void onTextChanged();
   void onUserUrlOpened(const QUrl &url);

private:
   //using MessagesHistory = std::vector<std::shared_ptr<Chat::Data_Message>>;
   using MessagesHistory = std::vector<std::shared_ptr<Chat::Data>>;
   QMap<std::string, MessagesHistory> messages_;
   MessagesHistory messagesToLoadMore_;
   std::string currentChatId_;
   std::string ownUserId_;
   std::string username_;
   ChatItemActionsHandler * handler_;
   std::shared_ptr<ChatMessageReadHandler> messageReadHandler_;
   std::shared_ptr<ChatClient> client_;

private:
   std::shared_ptr<Chat::Data> findMessage(const std::string& chatId, const std::string& messageId);
   void notifyMessageChanged(std::shared_ptr<Chat::Data> message);
   void insertMessage(std::shared_ptr<Chat::Data> message);
   void insertLoadMore();
   void loadMore();
   void setupHighlightPalette();
   void initUserContextMenu();
   QString toHtmlText(const QString &text);
   QString toHtmlUsername(const QString &username, const QString &userId = QString());
   QString toHtmlInvalid(const QString &text);

   QTextTableFormat tableFormat_;
   QTextTable *table_{};
   ChatMessagesTextEditStyle internalStyle_;

   QMenu *userMenu_{};
   QAction *userAddContactAction_{};
   QAction *userRemoveContactAction_{};
   bool isGroupRoom_{};
   bool isChatTab_{};

   QImage statusImageOffline_;
   QImage statusImageConnecting_;
   QImage statusImageOnline_;
   QImage statusImageRead_;

   // ViewItemWatcher interface
public:
   void onElementSelected(CategoryElement *element) override;
   void onMessageChanged(std::shared_ptr<Chat::Data> message) override;
   void onElementUpdated(CategoryElement *element) override;
   void onCurrentElementAboutToBeRemoved() override;
   QTextCursor textCursor_;
   QString anchor_;
};
#endif
