#ifndef CHATMESSAGESTEXTEDIT_H
#define CHATMESSAGESTEXTEDIT_H

#include "ChatProtocol/Message.h"
#include "ChatProtocol/ClientPartyModel.h"

#include <QDateTime>
#include <QMenu>
#include <QTextBrowser>
#include <QTextTable>
#include <QVector>

#include <memory>

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
   Q_PROPERTY(QColor color_red READ colorRed
              WRITE setColorRed)
   Q_PROPERTY(QColor color_otc READ colorOtc
              WRITE setColorOtc)

public:
   inline explicit ChatMessagesTextEditStyle(QWidget *parent)
      : QWidget(parent), colorHyperlink_(Qt::blue), colorWhite_(Qt::white), colorRed_(Qt::red), colorOtc_(Qt::lightGray)
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

   QColor colorRed() const { return colorRed_; }
   void setColorRed(QColor val) { colorRed_ = val; }

   QColor colorOtc() const { return colorOtc_; }
   void setColorOtc(const QColor &colorOtc) {
      colorOtc_ = colorOtc;
   }

private:
   QColor colorHyperlink_;
   QColor colorWhite_;
   QColor colorRed_;
   QColor colorOtc_;
};

class ChatMessagesTextEdit : public QTextBrowser
{
   Q_OBJECT

public:
   ChatMessagesTextEdit(QWidget* parent = nullptr);
   ~ChatMessagesTextEdit() noexcept override = default;

public:
   QString getFormattedTextFromSelection() const;

public slots:
   void onSetColumnsWidth(int time, int icon, int user, int message);
   void onSetOwnUserId(const std::string &userId) { ownUserId_ = userId; }
   void onSetClientPartyModel(const Chat::ClientPartyModelPtr& partyModel);
   void onSwitchToChat(const std::string& partyId);
   void onLogout();
   Chat::MessagePtr onMessageStatusChanged(const std::string& partyId, const std::string& message_id,
                                           const int party_message_state);
   void onMessageUpdate(const Chat::MessagePtrList& messagePtrList);
   void onUpdatePartyName(const std::string& partyId);

   void onShowRequestPartyBox(const std::string& userHash);

signals:
   void messageRead(const std::string& partyId, const std::string& messageId);
   void newPartyRequest(const std::string& userName, const std::string& initialMessage);
   void removePartyRequest(const std::string& partyId);
   void switchPartyRequest(const QString& partyId);

protected:
   enum class Column {
      Time,
      Status,
      User,
      Message,
      last
   };

   QString data(const std::string& partyId, const std::string& messageId, const Column &column);
   QString dataMessage(const std::string& partyId, const std::string& messageId, const Column &column) const;
   QImage statusImage(const std::string& partyId, const std::string& messageId) const;

   void contextMenuEvent(QContextMenuEvent* e) override;

private slots:
   void onUrlActivated(const QUrl &link);
   void onCopyActionTriggered() const;
   void onCopyLinkLocationActionTriggered() const;
   void onSelectAllActionTriggered();
   void onTextChanged() const;
   void onUserUrlOpened(const QUrl &url);

private:
   Chat::MessagePtr getMessage(const std::string& partyId, const std::string& messageId) const;
   void setupHighlightPalette();
   std::unique_ptr<QMenu> initUserContextMenu(const QString& userName);

   // #new_logic
   QString toHtmlUsername(const std::string& username, const std::string& userId) const;
   QString toHtmlText(const QString &text) const;
   QString toHtmlInvalid(const QString &text) const;

   void insertMessage(const Chat::MessagePtr& messagePtr);
   void showMessage(const std::string& partyId, const std::string& messageId);
   void showMessages(const std::string& partyId);
   Chat::MessagePtr findMessage(const std::string& partyId, const std::string& messageId);
   void notifyMessageChanged(const Chat::MessagePtr& message);
   void insertMessageInDoc(QTextCursor& cursor, const std::string& partyId, const std::string& messageId);
   void updateMessage(const std::string& partyId, const std::string& messageId);
   QTextCursor deleteMessage(int index) const;
   QString elideUserName(const std::string& displayName) const;

   Chat::ClientPartyModelPtr partyModel_;

   std::string currentPartyId_;
   std::string ownUserId_;

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
   int userColumnWidth_ = 0;
};

#endif // CHATMESSAGESTEXTEDIT_H
