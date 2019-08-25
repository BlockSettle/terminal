#ifndef __CHAT_MESSAGES_VIEW_MODEL_H__
#define __CHAT_MESSAGES_VIEW_MODEL_H__

#include "ChatClient.h"
#include "ChatClientUserView.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/ClientPartyModel.h"

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

// #new_logic : left as it is
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

// #new_logic : redoing
class ChatMessagesTextEdit : public QTextBrowser
{
   Q_OBJECT

public:
   ChatMessagesTextEdit(QWidget* parent = nullptr);
   ~ChatMessagesTextEdit() noexcept override = default;

public:
   // #old_logic
   
   void setHandler(ChatItemActionsHandler* handler);
   void setMessageReadHandler(std::shared_ptr<ChatMessageReadHandler> handler);
   
   QString getFormattedTextFromSelection();

   // #new_logic
   void setColumnsWidth(const int &time, const int &icon, const int &user, const int &message);
   void setOwnUserId(const std::string &userId) { ownUserId_ = userId; }
   void setClientPartyModel(Chat::ClientPartyModelPtr partyModel);
   void switchToChat(const std::string& chatId);
   void resetChatView();
   void logout();

signals:
   void MessageRead(const std::shared_ptr<Chat::Data> &) const;
   void sendFriendRequest(const std::string &userID);
   void addContactRequired(const QString &userId);

   // #new_logic
   void messageAdded();

protected:
   enum class Column {
      Time,
      Status,
      User,
      Message,
      last
   };

   // #old_logic
   QString data(int row, const Column &column);
   QString dataMessage(int row, const Column &column);
   

   void contextMenuEvent(QContextMenuEvent *e) override;

   // #new_logic
   QImage statusImage(int row);

public slots:
   // #old_logic
   void onMessageIdUpdate(const std::string& oldId, const std::string& newId,const std::string& chatId);
   void onMessageStatusChanged(const std::string& messageId, const std::string &chatId, int newStatus);
   void urlActivated(const QUrl &link);

   // #new_logic
   void onSingleMessageUpdate(const Chat::MessagePtrList& messagePtrList);
   void onCurrentElementAboutToBeRemoved();
   
private slots:
   void copyActionTriggered();
   void copyLinkLocationActionTriggered();
   void selectAllActionTriggered();
   void onTextChanged();
   void onUserUrlOpened(const QUrl &url);

private:
   // #old_logic 
   //using MessagesHistory = std::vector<std::shared_ptr<Chat::Data_Message>>;
   
   // MessagesHistory messagesToLoadMore_;
   //std::string currentChatId_;
   //std::string ownUserId_;
   //std::string username_;
   ChatItemActionsHandler * handler_;
   std::shared_ptr<ChatMessageReadHandler> messageReadHandler_;
   std::shared_ptr<ChatClient> client_;

   // #new_logic : comment and move variables from previous section which is needed
   Chat::ClientPartyModelPtr partyModel_;

   std::string currentChatId_;
   std::string ownUserId_;
   std::string username_;

   using ClientMessagesHistory = QVector<Chat::MessagePtr>;
   QMap<std::string, ClientMessagesHistory> messages_;

   QImage statusImageOffline_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_OFFLINE") }, "PNG");
   QImage statusImageConnecting_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_CONNECTING") }, "PNG");
   QImage statusImageOnline_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_ONLINE") }, "PNG");
   QImage statusImageRead_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_READ") }, "PNG");

private:
   // #old_logic : 
   std::shared_ptr<Chat::Data> findMessage(const std::string& chatId, const std::string& messageId);
   void notifyMessageChanged(std::shared_ptr<Chat::Data> message);

   void insertLoadMore();
   void loadMore();
   void setupHighlightPalette();
   void initUserContextMenu(); // #old_logic : do we need it?

   // #new_logic
   QString toHtmlUsername(const QString &username, const QString &userId = QString());
   QString toHtmlText(const QString &text);
   QString toHtmlInvalid(const QString &text);
   void insertMessage(const Chat::MessagePtr& messagePtr);
   void showMessage(int messageIndex);
   void forceMessagesUpdate();

private:
   QTextTableFormat tableFormat_;
   ChatMessagesTextEditStyle internalStyle_;

   QMenu *userMenu_{};
   QAction *userAddContactAction_{};
   QAction *userRemoveContactAction_{};

   // ViewItemWatcher interface
public:
   // #old_logic
   //void onElementSelected(CategoryElement *element) override;
   //void onMessageChanged(std::shared_ptr<Chat::Data> message) override;
   //void onElementUpdated(CategoryElement *element) override;
   //void onCurrentElementAboutToBeRemoved() override;
   QTextCursor textCursor_;
   QString anchor_;
};
#endif
