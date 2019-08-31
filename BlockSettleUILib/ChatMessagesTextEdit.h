#ifndef __CHAT_MESSAGES_VIEW_MODEL_H__
#define __CHAT_MESSAGES_VIEW_MODEL_H__

#include "ChatClient.h"
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

class ChatMessagesTextEdit : public QTextBrowser
{
   Q_OBJECT

public:
   ChatMessagesTextEdit(QWidget* parent = nullptr);
   ~ChatMessagesTextEdit() noexcept override = default;

public:
   QString getFormattedTextFromSelection();

public slots:
   void setColumnsWidth(const int &time, const int &icon, const int &user, const int &message);
   void setOwnUserId(const std::string &userId) { ownUserId_ = userId; }
   void setClientPartyModel(Chat::ClientPartyModelPtr partyModel);
   void switchToChat(const std::string& partyId);
   void logout();
   const Chat::MessagePtr onMessageStatusChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
   void onMessageUpdate(const Chat::MessagePtrList& messagePtrList);

signals:
   void messageRead(const std::string& partyId, const std::string& messageId);
   void newPartyRequest(const std::string& userName);
   void removePartyRequest(const std::string& partyId);

protected:
   enum class Column {
      Time,
      Status,
      User,
      Message,
      last
   };

   QString data(const std::string& partyId, int row, const Column &column);
   QString dataMessage(const std::string& partyId, int row, const Column &column);
   QImage statusImage(const std::string& partyId, int row);

   void contextMenuEvent(QContextMenuEvent *e) override;

private slots:
   void urlActivated(const QUrl &link);
   void copyActionTriggered();
   void copyLinkLocationActionTriggered();
   void selectAllActionTriggered();
   void onTextChanged();
   void onUserUrlOpened(const QUrl &url);

private:
   void setupHighlightPalette();
   QMenu* initUserContextMenu(const QString& userName);

   // #new_logic
   QString toHtmlUsername(const QString &username, const QString &userId = QString());
   QString toHtmlText(const QString &text);
   QString toHtmlInvalid(const QString &text);

   void insertMessage(const Chat::MessagePtr& messagePtr);
   void showMessage(const std::string& partyId, int messageIndex);
   void showMessages(const std::string& partyId);
   Chat::MessagePtr findMessage(const std::string& partyId, const std::string& messageId);
   void notifyMessageChanged(Chat::MessagePtr message);
   void insertMessageInDoc(QTextCursor& cursor, const std::string& partyId, int index);

private:
   Chat::ClientPartyModelPtr partyModel_;

   std::string currentPartyId_;
   std::string ownUserId_;
   std::string userName_;

   using ClientMessagesHistory = QVector<Chat::MessagePtr>;
   QMap<std::string, ClientMessagesHistory> messages_;

   QImage statusImageGreyUnsent_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_OFFLINE") }, "PNG");
   QImage statusImageYellowSent_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_CONNECTING") }, "PNG");
   QImage statusImageGreenReceived_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_ONLINE") }, "PNG");
   QImage statusImageBlueSeen_ = QImage({ QLatin1Literal(":/ICON_MSG_STATUS_READ") }, "PNG");

   QTextTableFormat tableFormat_;
   ChatMessagesTextEditStyle internalStyle_;

   QTextCursor textCursor_;
   QString anchor_;
};
#endif
