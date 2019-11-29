/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatMessagesTextEdit.h"

#include "OtcUtils.h"
#include "ProtobufUtils.h"
#include "RequestPartyBox.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QMimeData>
#include <QScrollBar>

#include <iterator>

namespace {
   // Translation
   const QString ownSenderUserName = QObject::tr("you");
   const QString contextMenuCopy = QObject::tr("Copy");
   const QString contextMenuCopyLink = QObject::tr("Copy Link Location");
   const QString contextMenuSelectAll= QObject::tr("Select All");
   const QString contextMenuAddUserMenu = QObject::tr("Add to contacts");
   const QString contextMenuAddUserMenuStatusTip = QObject::tr("Click to add user to contact list");
   const QString contextMenuRemoveUserMenu = QObject::tr("Remove from contacts");
   const QString contextMenuRemoveUserMenuStatusTip = QObject::tr("Click to remove user from contact list");
}

ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextBrowser(parent)
   , internalStyle_(this)
{
   tableFormat_.setBorder(0);
   tableFormat_.setCellPadding(0);
   tableFormat_.setCellSpacing(0);

   setupHighlightPalette();

   connect(this, &QTextBrowser::anchorClicked, this, &ChatMessagesTextEdit::onUrlActivated);
   connect(this, &QTextBrowser::textChanged, this, &ChatMessagesTextEdit::onTextChanged);
}

void ChatMessagesTextEdit::setupHighlightPalette()
{
   auto highlightPalette = palette();
   highlightPalette.setColor(QPalette::Inactive, QPalette::Highlight, highlightPalette.color(QPalette::Active, QPalette::Highlight));
   highlightPalette.setColor(QPalette::Inactive, QPalette::HighlightedText, highlightPalette.color(QPalette::Active, QPalette::HighlightedText));
   setPalette(highlightPalette);
}

Chat::MessagePtr ChatMessagesTextEdit::getMessage(const std::string& partyId, const std::string& messageId) const
{
   const auto it = std::find_if(messages_[partyId].begin(), messages_[partyId].end(), [messageId](const Chat::MessagePtr& msg)->bool {
      return msg->messageId() == messageId;
   });

   if (it != messages_[partyId].end()) {
      return *it;
   }

   return nullptr;
}

QString ChatMessagesTextEdit::data(const std::string& partyId, const std::string& messageId, const Column &column)
{
   if (messages_[partyId].empty()) {
       return QString();
   }

   return dataMessage(partyId, messageId, column);
}

QString ChatMessagesTextEdit::dataMessage(const std::string& partyId, const std::string& messageId, const ChatMessagesTextEdit::Column &column) const
{
   const auto message = getMessage(partyId, messageId);

   if (!message) {
      return QString();
   }

   Chat::ClientPartyPtr previousClientPartyPtr = nullptr;

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = message->timestamp().toLocalTime();
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         const auto& senderHash = message->senderHash();

         if (senderHash == ownUserId_) {
            return ownSenderUserName;
         }

         if (!previousClientPartyPtr || previousClientPartyPtr->id() != partyId) {
            previousClientPartyPtr = partyModel_->getClientPartyById(message->partyId());
         }

         if (!previousClientPartyPtr->isGlobal()) {
            return elideUserName(previousClientPartyPtr->displayName());
         }
         
         const auto clientPartyPtr = partyModel_->getClientPartyById(partyId);

         if (clientPartyPtr && clientPartyPtr->isPrivate()) {
            return toHtmlUsername(clientPartyPtr->displayName(), clientPartyPtr->userHash());
         }
         return toHtmlUsername(senderHash, senderHash);
      }
      case Column::Status:{
         return QString();
      }
      case Column::Message: {
         //QString text = QLatin1String("[%1] %2");
         //text = text.arg(QString::fromStdString(message->messageText()));

         //if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_INVALID)) {
         //   return toHtmlInvalid(text.arg(QLatin1String("INVALID MESSAGE!")));
         //} else if (message.encryption() == Chat::Data_Message_Encryption_IES) {
         //   return toHtmlInvalid(text.arg(QLatin1String("IES ENCRYPTED!")));
         //} else if ( message.encryption() == Chat::Data_Message_Encryption_AEAD) {
         //   return toHtmlInvalid(text.arg(QLatin1String("AEAD ENCRYPTED!")));
         //}
         return toHtmlText(QString::fromStdString(message->messageText()));
      }
      default:
         break;
   }

   return QString();
}

QImage ChatMessagesTextEdit::statusImage(const std::string& partyId, const std::string& messageId) const
{
   const auto message = getMessage(partyId, messageId);

   if (!message) {
      return statusImageGreyUnsent_;
   }

   if (message->senderHash() != ownUserId_) {
      return QImage();
   }

   auto statusImage = statusImageGreyUnsent_;

   const auto clientPartyPtr = partyModel_->getClientPartyById(message->partyId());
   
   if (!clientPartyPtr) {
      return QImage();
   }

   if (clientPartyPtr->isGlobalStandard()) {
      if ((message->partyMessageState() != Chat::PartyMessageState::UNSENT)) {
         statusImage = statusImageBlueSeen_;
      }
      return statusImage;
   }

   if (message->partyMessageState() == Chat::PartyMessageState::UNSENT) {
      statusImage = statusImageGreyUnsent_;
   } else if (message->partyMessageState() == Chat::PartyMessageState::SENT) {
      statusImage = statusImageYellowSent_;
   } else if (message->partyMessageState() == Chat::PartyMessageState::RECEIVED) {
      statusImage = statusImageGreenReceived_;
   } else if (message->partyMessageState() == Chat::PartyMessageState::SEEN) {
      statusImage = statusImageBlueSeen_;
   }

   return statusImage;
}

void ChatMessagesTextEdit::contextMenuEvent(QContextMenuEvent *e)
{
   textCursor_ = cursorForPosition(e->pos());

   // keep selection
   if (textCursor().hasSelection()) {
      textCursor_.setPosition(textCursor().selectionStart(), QTextCursor::MoveAnchor);
      textCursor_.setPosition(textCursor().selectionEnd(), QTextCursor::KeepAnchor);
   }

   setTextCursor(textCursor_);
   QString text = textCursor_.block().text();

   //show contact context menu when username is right clicked in User column
   if ((textCursor_.block().blockNumber() - 1) % 5 == static_cast<int>(Column::User)) {
      if (text != ownSenderUserName) {
         const QUrl link = anchorAt(e->pos());
         if (!link.isEmpty()) {
            text = link.path();
         }
         std::unique_ptr<QMenu> userMenuPtr = initUserContextMenu(text);
         userMenuPtr->exec(QCursor::pos());
         return;
      }
   }

   // show default text context menu
   if (text.length() > 0 || textCursor_.hasSelection()) {
      QMenu contextMenu(this);

      QAction copyAction(contextMenuCopy, this);
      QAction copyLinkLocationAction(contextMenuCopyLink, this);
      QAction selectAllAction(contextMenuSelectAll, this);

      connect(&copyAction, &QAction::triggered, this, &ChatMessagesTextEdit::onCopyActionTriggered);
      connect(&copyLinkLocationAction, &QAction::triggered, this, &ChatMessagesTextEdit::onCopyLinkLocationActionTriggered);
      connect(&selectAllAction, &QAction::triggered, this, &ChatMessagesTextEdit::onSelectAllActionTriggered);

      contextMenu.addAction(&copyAction);

      // show Copy Link Location only when it needed
      anchor_ = this->anchorAt(e->pos());
      if (!anchor_.isEmpty()) {
         contextMenu.addAction(&copyLinkLocationAction);
      }

      contextMenu.addSeparator();
      contextMenu.addAction(&selectAllAction);

      contextMenu.exec(e->globalPos());
   }
}

void ChatMessagesTextEdit::onCopyActionTriggered() const
{
   if (textCursor_.hasSelection()) {
      QApplication::clipboard()->setText(getFormattedTextFromSelection());
   }
   else {
      QTextDocument doc;
      doc.setHtml(textCursor_.block().text());
      QApplication::clipboard()->setText(doc.toPlainText());
   }
}

void ChatMessagesTextEdit::onCopyLinkLocationActionTriggered() const
{
   QApplication::clipboard()->setText(anchor_);
}

void ChatMessagesTextEdit::onSelectAllActionTriggered()
{
   this->selectAll();
}

void ChatMessagesTextEdit::onTextChanged() const
{
   verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void ChatMessagesTextEdit::onUserUrlOpened(const QUrl &url)
{
   const std::string userId = url.path().toStdString();
   const Chat::ClientPartyPtr clientPartyPtr = partyModel_->getStandardPartyForUsers(ownUserId_, userId);

   if (!clientPartyPtr) {
      onShowRequestPartyBox(userId);
      return;
   }

   if (Chat::PartyState::REJECTED == clientPartyPtr->partyState()) {
      onShowRequestPartyBox(clientPartyPtr->userHash());
      return;
   }

   if (clientPartyPtr->id() == currentPartyId_) {
      return;
   }

   emit switchPartyRequest(QString::fromStdString(clientPartyPtr->id()));
}

void ChatMessagesTextEdit::onShowRequestPartyBox(const std::string& userHash)
{
   std::string requestUserHash = userHash;

   const Chat::ClientPartyPtr clientPartyPtr = partyModel_->getStandardPartyForUsers(ownUserId_, requestUserHash);

   const QString requestNote = tr("You can enter initial message below:");
   const QString requestTitle = tr("Do you want to send friend request to %1 ?").arg(QString::fromStdString(requestUserHash));

   RequestPartyBox rpBox(requestTitle, requestNote);
   if (rpBox.exec() == QDialog::Accepted) {
      if (clientPartyPtr && Chat::PartyState::REJECTED == clientPartyPtr->partyState()) {
         emit removePartyRequest(clientPartyPtr->id());
         requestUserHash = clientPartyPtr->userHash();
      }

      emit newPartyRequest(requestUserHash, rpBox.getCustomMessage().toStdString());
   }
}

void ChatMessagesTextEdit::onSwitchToChat(const std::string& partyId)
{
   currentPartyId_ = partyId;
   clear();
   if (!currentPartyId_.empty()) {
      showMessages(partyId);
      onTextChanged();
      auto clientMessagesHistory = messages_[partyId];

      if (clientMessagesHistory.empty()) {
         return;
      }

      auto riter = clientMessagesHistory.rbegin();
      for (; riter != clientMessagesHistory.rend(); ++riter)
      {
         const auto messagePtr = (*riter);
         
         if (messagePtr->partyMessageState() == Chat::PartyMessageState::SEEN) {
            continue;
         }

         if (messagePtr->senderHash() == ownUserId_) {
            continue;
         }

         emit messageRead(messagePtr->partyId(), messagePtr->messageId());
      }
   }
}

void ChatMessagesTextEdit::onLogout()
{
   onSwitchToChat({});
}

Chat::MessagePtr ChatMessagesTextEdit::onMessageStatusChanged(const std::string& partyId, const std::string& message_id,
                                                              const int party_message_state)
{
   Chat::MessagePtr message = findMessage(partyId, message_id);

   if (message) {
      message->setPartyMessageState(static_cast<Chat::PartyMessageState>(party_message_state));
      notifyMessageChanged(message);
   }

   return message;
}

void ChatMessagesTextEdit::onSetColumnsWidth(int time, int icon, int user, int message)
{
   QVector <QTextLength> col_widths;
   col_widths << QTextLength(QTextLength::FixedLength, time);
   col_widths << QTextLength(QTextLength::FixedLength, icon);
   col_widths << QTextLength(QTextLength::FixedLength, user);
   col_widths << QTextLength(QTextLength::VariableLength, message);
   tableFormat_.setColumnWidthConstraints(col_widths);
   userColumnWidth_ = user;
}

void ChatMessagesTextEdit::onSetClientPartyModel(const Chat::ClientPartyModelPtr& partyModel)
{
   partyModel_ = partyModel;
}

QString ChatMessagesTextEdit::getFormattedTextFromSelection() const
{
   QString text;
   QTextDocument textDocument;

   // get selected text in html format
   textDocument.setHtml(createMimeDataFromSelection()->html());      
   QTextBlock currentBlock = textDocument.begin();
   int blockCount = 0;

   // each column is presented as a block
   while (currentBlock.isValid()) {
      blockCount++;
      if (!currentBlock.text().isEmpty()) {

         // format columns splits to tabulation
         if (!text.isEmpty()) {
            text += QChar::Tabulation;
            
            // new row (when few rows are selected)
            if ((blockCount - 2) % 5 == 0) {
               text += QChar::LineFeed;
            }
         }
         // replace some special characters, because they can display incorrect
         text += currentBlock.text().replace(QChar::LineSeparator, QChar::LineFeed);
      }
      currentBlock = currentBlock.next();
   }
   return text;
}

void  ChatMessagesTextEdit::onUrlActivated(const QUrl &link) {
   if (link.scheme() != QLatin1Literal("user")) {
      QDesktopServices::openUrl(link);
   }
   else {
      onUserUrlOpened(link);
   }
}

void ChatMessagesTextEdit::insertMessage(const Chat::MessagePtr& messagePtr)
{
   // push new message if it doesn't exist in current chat
   auto& messagesList = messages_[messagePtr->partyId()];
   const auto messageIt =
   std::find_if(messagesList.begin(), messagesList.end(), [messagePtr](const Chat::MessagePtr& m)->bool
   {
      return m->messageId() == messagePtr->messageId();
   });

   // remove duplicates by give message_id
   if (messageIt != messagesList.cend())
   {
      deleteMessage(static_cast<int>(std::distance(messagesList.begin(), messageIt)));
      messagesList.erase(messageIt);
   }

   messagesList.push_back(messagePtr);
   if (messagePtr->partyId() == currentPartyId_) {
      showMessage(messagePtr->partyId(), messagePtr->messageId());
   }

   if (messagePtr->partyMessageState() != Chat::PartyMessageState::SEEN
         && messagePtr->senderHash() != ownUserId_
         && messagePtr->partyId() == currentPartyId_
         && isVisible()) {
      emit messageRead(messagePtr->partyId(), messagePtr->messageId());
   }
}

void ChatMessagesTextEdit::insertMessageInDoc(QTextCursor& cursor, const std::string& partyId, const std::string& messageId)
{
   cursor.beginEditBlock();
   auto* table = cursor.insertTable(1, 4, tableFormat_);

   const auto time = data(partyId, messageId, Column::Time);
   table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

   const auto image = statusImage(partyId, messageId);
   if (!image.isNull()) {
      table->cellAt(0, 1).firstCursorPosition().insertImage(image);
   }

   const auto user = data(partyId, messageId, Column::User);
   table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

   const auto message = data(partyId, messageId, Column::Message);
   table->cellAt(0, 3).firstCursorPosition().insertHtml(message);
   cursor.endEditBlock();
}

void ChatMessagesTextEdit::updateMessage(const std::string& partyId, const std::string& messageId)
{
   ClientMessagesHistory messagesList = messages_[partyId];
   const ClientMessagesHistory::iterator it =
      std::find_if(messagesList.begin(), messagesList.end(), [messageId](const Chat::MessagePtr& m)->bool
   {
      return m->messageId() == messageId;
   });

   if (it != messagesList.end())
   {
      const int distance = std::distance(messagesList.begin(), it);
      auto cursor = deleteMessage(distance);
      insertMessageInDoc(cursor, partyId, messageId);
   }
}

QTextCursor ChatMessagesTextEdit::deleteMessage(const int index) const
{
   QTextCursor cursor = textCursor();
   cursor.movePosition(QTextCursor::Start);
   cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, index * 2);
   cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 1);
   cursor.removeSelectedText();

   return cursor;
}

QString ChatMessagesTextEdit::elideUserName(const std::string& displayName) const
{
   return fontMetrics().elidedText(QString::fromStdString(displayName), Qt::ElideRight, userColumnWidth_);
}

void ChatMessagesTextEdit::showMessage(const std::string& partyId, const std::string& messageId)
{
   /* add text */
   QTextCursor cursor = textCursor();
   cursor.movePosition(QTextCursor::End);

   insertMessageInDoc(cursor, partyId, messageId);
}

void ChatMessagesTextEdit::showMessages(const std::string &partyId)
{
   for (const auto& message : messages_[partyId]) {
      showMessage(partyId, message->messageId());
   }
}

std::unique_ptr<QMenu> ChatMessagesTextEdit::initUserContextMenu(const QString& userName)
{
   std::unique_ptr<QMenu> userMenuPtr = std::make_unique<QMenu>(this);

   Chat::ClientPartyPtr clientPartyPtr = partyModel_->getStandardPartyForUsers(ownUserId_, userName.toStdString());
   if (!clientPartyPtr) {
      QAction* addAction = userMenuPtr->addAction(contextMenuAddUserMenu);
      addAction->setStatusTip(contextMenuAddUserMenuStatusTip);
      connect(addAction, &QAction::triggered, this, [this, userName_ = userName]() {
         onShowRequestPartyBox(userName_.toStdString());
      });
   }
   else {
      QAction* removeAction = userMenuPtr->addAction(contextMenuRemoveUserMenu);
      removeAction->setStatusTip(contextMenuRemoveUserMenuStatusTip);
      connect(removeAction, &QAction::triggered, this, [this, clientPartyPtr]() {
         emit removePartyRequest(clientPartyPtr->id());
      });
   }

   return userMenuPtr;
}

void ChatMessagesTextEdit::onMessageUpdate(const Chat::MessagePtrList& messagePtrList)
{
#ifndef QT_NO_DEBUG
   const std::string& partyId = !messagePtrList.empty() ? messagePtrList[0]->partyId() : "";
#endif
   Chat::MessagePtrList messagePtrListSorted = messagePtrList;
   std::sort(messagePtrListSorted.begin(), messagePtrListSorted.end(), [](const auto& left, const auto& right) -> bool {
      return left->timestamp() < right->timestamp();
   });
   for (const auto& messagePtr : messagePtrListSorted) {
#ifndef QT_NO_DEBUG
      Q_ASSERT(partyId == messagePtr->partyId());
#endif
      insertMessage(messagePtr);
   }
}

void ChatMessagesTextEdit::onUpdatePartyName(const std::string& partyId)
{
   const auto messageHistory = messages_[currentPartyId_];
   
   for (const auto& messagePtr : messageHistory)
   {
      if (messagePtr->partyId() != partyId) {
         continue;
      }

      updateMessage(partyId, messagePtr->messageId());
   }
}

Chat::MessagePtr ChatMessagesTextEdit::findMessage(const std::string& partyId, const std::string& messageId)
{
   if (messages_.contains(partyId)) {
      const auto it = std::find_if(messages_[partyId].begin(), messages_[partyId].end(), [messageId](const Chat::MessagePtr& message) {
         return message->messageId() == messageId;
      });

      if (it != messages_[partyId].end()) {
         return *it;
      }
   }

   return {};
}

void ChatMessagesTextEdit::notifyMessageChanged(const Chat::MessagePtr& message)
{
   const std::string& partyId = message->partyId();
   if (partyId != currentPartyId_) {
      // Do not need to update view
      return;
   }

   if (messages_.contains(partyId)) {
      const std::string& id = message->messageId();
      const auto it = std::find_if(messages_[partyId].begin(), messages_[partyId].end(), [id](const Chat::MessagePtr& iteration) {
         return iteration->messageId() == id;
      });

      if (it != messages_[partyId].end()) {
         updateMessage(partyId, (*it)->messageId());
      }
   }
}

QString ChatMessagesTextEdit::toHtmlUsername(const std::string& username, const std::string& userId) const
{
   return QStringLiteral("<a href=\"user:%1\" style=\"color:%2\">%3</a>")
      .arg(QString::fromStdString(userId))
      .arg(internalStyle_.colorHyperlink().name())
      .arg(elideUserName(username));
}

QString ChatMessagesTextEdit::toHtmlInvalid(const QString &text) const
{
   QString changedText = QStringLiteral("<font color=\"%1\">%2</font>").arg(internalStyle_.colorRed().name()).arg(text);
   return changedText;
}

QString ChatMessagesTextEdit::toHtmlText(const QString &text) const
{
   const auto otcText = OtcUtils::toReadableString(text);
   if (!otcText.isEmpty()) {
      // No further processing is needed
      return QStringLiteral("<font color=\"%1\">*** %2 ***</font>").arg(internalStyle_.colorOtc().name()).arg(otcText);
   }

   QString changedText = text;

   // make linkable
   int index = 0;
   int startIndex;

   while ((startIndex = changedText.indexOf(QLatin1Literal("https://"), index, Qt::CaseInsensitive)) != -1
      || (startIndex = changedText.indexOf(QLatin1Literal("http://"), index, Qt::CaseInsensitive)) != -1) {

      int endIndex = changedText.indexOf(QLatin1Literal(" "), startIndex);
      if (endIndex == -1) {
         endIndex = changedText.indexOf(QLatin1Literal("\n"), startIndex);
      }
      if (endIndex == -1) {
         endIndex = changedText.length();
      }

      QString linkText = changedText.mid(startIndex, endIndex - startIndex);
      QString hyperlinkText = QStringLiteral("<a href=\"%1\" style=\"color:%2\">%1</a>").arg(linkText).arg(internalStyle_.colorHyperlink().name());

      changedText = changedText.replace(startIndex, endIndex - startIndex, hyperlinkText);

      index = startIndex + hyperlinkText.length();
   }

   // replace linefeed with <br>
   changedText.replace(QLatin1Literal("\n"), QLatin1Literal("<br>"));

   // set text color as white
   changedText = QStringLiteral("<font color=\"%1\">%2</font>").arg(internalStyle_.colorWhite().name()).arg(changedText);

   return changedText;
}
