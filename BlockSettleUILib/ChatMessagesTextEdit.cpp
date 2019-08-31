#include "ChatMessagesTextEdit.h"

#include "ChatClientDataModel.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatProtocol/ChatUtils.h"
#include "ProtobufUtils.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QMimeData>
#include <QScrollBar>

#include <set>

namespace {
   const QString contextMenuCopy = QObject::tr("Copy");
   const QString contextMenuCopyLink = QObject::tr("Copy Link Location");
   const QString contextMenuSelectAll= QObject::tr("Select All");
}

ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextBrowser(parent)
   , internalStyle_(this)
{
   tableFormat_.setBorder(0);
   tableFormat_.setCellPadding(0);
   tableFormat_.setCellSpacing(0);

   setupHighlightPalette();

   connect(this, &QTextBrowser::anchorClicked, this, &ChatMessagesTextEdit::urlActivated);
   connect(this, &QTextBrowser::textChanged, this, &ChatMessagesTextEdit::onTextChanged);

   initUserContextMenu();
}

void ChatMessagesTextEdit::setupHighlightPalette()
{
   auto highlightPalette = palette();
   highlightPalette.setColor(QPalette::Inactive, QPalette::Highlight, highlightPalette.color(QPalette::Active, QPalette::Highlight));
   highlightPalette.setColor(QPalette::Inactive, QPalette::HighlightedText, highlightPalette.color(QPalette::Active, QPalette::HighlightedText));
   setPalette(highlightPalette);
}

QString ChatMessagesTextEdit::data(const std::string& partyId, int row, const Column &column)
{
   if (messages_[partyId].empty()) {
       return QString();
   }

   // TODO: Filter OTC messages
   return dataMessage(partyId, row, column);
}

QString ChatMessagesTextEdit::dataMessage(const std::string& partyId, int row, const ChatMessagesTextEdit::Column &column)
{
   const Chat::MessagePtr message = messages_[partyId][row];

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = QDateTime::fromMSecsSinceEpoch(message->timestamp()).toLocalTime();
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         const auto senderHash = message->senderHash();
         static const auto ownSender = tr("you");

         if (senderHash == ownUserId_) {
            return ownSender;
         }

         return toHtmlUsername(QString::fromStdString(senderHash));
      }
      case Column::Status:{
         return QString();
         break;
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

QImage ChatMessagesTextEdit::statusImage(const std::string& partyId, int row)
{
   auto message = messages_[partyId][row];
   if (!message) {
      return statusImageGreyUnsent_;
   }

   if (message->senderHash() != ownUserId_) {
      return QImage();
   }

   QImage statusImage = statusImageGreyUnsent_;

   Chat::ClientPartyPtr clientPartyPtr = partyModel_->getClientPartyById(message->partyId());
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
   std::string text = textCursor_.block().text().toStdString();

   // #old_logic
   // show contact context menu when username is right clicked in User column
   //if ((textCursor_.block().blockNumber() - 1) % 5 == static_cast<int>(Column::User) ) {
   //   if (!anchorAt(e->pos()).isEmpty()) {
   //      userName_ = text;

   //      if (handler_) {
   //         for (auto action : userMenu_->actions()) {
   //            userMenu_->removeAction(action);
   //         }
   //         if (handler_->onActionIsFriend(userName_)) {
   //            userMenu_->addAction(userRemoveContactAction_);
   //         }
   //         else {
   //            emit addContactRequired(QString::fromStdString(username_));
   //         }
   //         userMenu_->exec(QCursor::pos());
   //      }
   //      return;
   //   }
   //}

   // show default text context menu
   if (text.length() > 0 || textCursor_.hasSelection()) {
      QMenu contextMenu(this);

      QAction copyAction(contextMenuCopy, this);
      QAction copyLinkLocationAction(contextMenuCopyLink, this);
      QAction selectAllAction(contextMenuSelectAll, this);

      connect(&copyAction, &QAction::triggered, this, &ChatMessagesTextEdit::copyActionTriggered);
      connect(&copyLinkLocationAction, &QAction::triggered, this, &ChatMessagesTextEdit::copyLinkLocationActionTriggered);
      connect(&selectAllAction, &QAction::triggered, this, &ChatMessagesTextEdit::selectAllActionTriggered);

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

void ChatMessagesTextEdit::copyActionTriggered()
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

void ChatMessagesTextEdit::copyLinkLocationActionTriggered()
{
   QApplication::clipboard()->setText(anchor_);
}

void ChatMessagesTextEdit::selectAllActionTriggered()
{
   this->selectAll();
}

void ChatMessagesTextEdit::onTextChanged()
{
   verticalScrollBar()->setValue(verticalScrollBar()->maximum());
}

void ChatMessagesTextEdit::onUserUrlOpened(const QUrl &url)
{
   // #old_logic
   userName_ = url.path().toStdString();
   //if (!handler_->onActionIsFriend(username_)) {
   //   emit addContactRequired(QString::fromStdString(username_));
   //}
}

void ChatMessagesTextEdit::switchToChat(const std::string& partyId)
{
   currentPartyId_ = partyId;
   clear();
   if (!currentPartyId_.empty()) {
      showMessages(partyId);
      onTextChanged();
      if (!messages_[partyId].isEmpty()) {
         for (auto iLast = messages_[partyId].end() - 1;
            iLast != messages_[partyId].begin() && (*iLast)->partyMessageState() != Chat::PartyMessageState::SEEN && (*iLast)->senderHash() != ownUserId_;
            --iLast) {
            emit messageRead((*iLast)->partyId(), (*iLast)->messageId());
         }
      }
   }
}

void ChatMessagesTextEdit::resetChatView()
{
   switchToChat(currentPartyId_);
}

void ChatMessagesTextEdit::logout()
{
   resetChatView();
   messages_.clear();
}

const Chat::MessagePtr ChatMessagesTextEdit::onMessageStatusChanged(const std::string& partyId, const std::string& message_id, const int party_message_state)
{
   Chat::MessagePtr message = findMessage(partyId, message_id);

   if (message) {
      message->setPartyMessageState(static_cast<Chat::PartyMessageState>(party_message_state));
      notifyMessageChanged(message);
   }

   return message;
}

void ChatMessagesTextEdit::setColumnsWidth(const int &time, const int &icon, const int &user, const int &message)
{
   QVector <QTextLength> col_widths;
   col_widths << QTextLength(QTextLength::FixedLength, time);
   col_widths << QTextLength(QTextLength::FixedLength, icon);
   col_widths << QTextLength(QTextLength::FixedLength, user);
   col_widths << QTextLength(QTextLength::VariableLength, message);
   tableFormat_.setColumnWidthConstraints(col_widths);
}

void ChatMessagesTextEdit::setClientPartyModel(Chat::ClientPartyModelPtr partyModel)
{
   partyModel_ = partyModel;
}

QString ChatMessagesTextEdit::getFormattedTextFromSelection()
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

void  ChatMessagesTextEdit::urlActivated(const QUrl &link) {
   // #old_logic
   //if (link.toString() == QLatin1Literal("load_more")) {
   //   loadMore();
   //}
   //if (link.scheme() != QLatin1Literal("user")) {
   //   QDesktopServices::openUrl(link);
   //} else {
   //   onUserUrlOpened(link);
   //}
}

void ChatMessagesTextEdit::insertMessage(const Chat::MessagePtr& messagePtr)
{
   const int messageIndex = messages_[messagePtr->partyId()].size();
   messages_[messagePtr->partyId()].push_back(messagePtr);

   if (messagePtr->partyId() == currentPartyId_) {
      showMessage(messagePtr->partyId(), messageIndex);
   }

   if (messagePtr->partyMessageState() != Chat::PartyMessageState::SEEN
         && messagePtr->senderHash() != ownUserId_
         && messagePtr->partyId() == currentPartyId_
         && isVisible()) {
      emit messageRead(messagePtr->partyId(), messagePtr->messageId());
   }
}

void ChatMessagesTextEdit::insertMessageInDoc(QTextCursor& cursor, const std::string& partyId, int index)
{
   cursor.beginEditBlock();
   auto* table = cursor.insertTable(1, 4, tableFormat_);

   QString time = data(partyId, index, Column::Time);
   table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

   QImage image = statusImage(partyId, index);
   if (!image.isNull()) {
      table->cellAt(0, 1).firstCursorPosition().insertImage(image);
   }

   QString user = data(partyId, index, Column::User);
   table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

   QString message = data(partyId, index, Column::Message);
   table->cellAt(0, 3).firstCursorPosition().insertHtml(message);
   cursor.endEditBlock();
}

void ChatMessagesTextEdit::showMessage(const std::string& partyId, int messageIndex)
{
   /* add text */
   QTextCursor cursor = textCursor();
   cursor.movePosition(QTextCursor::End);

   insertMessageInDoc(cursor, partyId, messageIndex);
}

void ChatMessagesTextEdit::showMessages(const std::string &partyId)
{
   for (int iMessage = 0; iMessage < messages_[partyId].size(); ++iMessage) {
      showMessage(partyId, iMessage);
   }
}

// #old_logic
//void ChatMessagesTextEdit::insertLoadMore()
//{
//   QTextCursor cursor(textCursor());
//   cursor.movePosition(QTextCursor::Start);
//   cursor.insertHtml(QString(QLatin1Literal("<a href=\"load_more\" style=\"color:%1\">Load More...</a>")).arg(internalStyle_.colorHyperlink().name()));
//}

void ChatMessagesTextEdit::initUserContextMenu()
{
   userMenu_ = new QMenu(this);

   userAddContactAction_ = new QAction(tr("Add to contacts"));
   userAddContactAction_->setStatusTip(tr("Click to add user to contact list"));
   connect(userAddContactAction_, &QAction::triggered, [this](bool) {
      //handler_->onActionCreatePendingOutgoing(username_);
   });

   userRemoveContactAction_ = new QAction(tr("Remove from contacts"));
   userRemoveContactAction_->setStatusTip(tr("Click to remove user from contact list"));
   connect(userRemoveContactAction_, &QAction::triggered, [this](bool) {
      // TODO:
      //handler_->onActionRemoveFromContacts(username_);
   });
}

void ChatMessagesTextEdit::onMessageUpdate(const Chat::MessagePtrList& messagePtrList)
{
#ifndef QT_NO_DEBUG
   const std::string& partyId = !messagePtrList.empty() ? messagePtrList[0]->partyId() : "";
#endif
   Chat::MessagePtrList messagePtrListSorted = messagePtrList;
   std::sort(messagePtrListSorted.begin(), messagePtrListSorted.end(), [](const auto left, const auto right) -> bool {
      return left->timestamp() < right->timestamp();
   });
   for (const auto& messagePtr : messagePtrListSorted)
   {
#ifndef QT_NO_DEBUG
      Q_ASSERT(partyId == messagePtr->partyId());
#endif
      insertMessage(messagePtr);
   }
}

Chat::MessagePtr ChatMessagesTextEdit::findMessage(const std::string& partyId, const std::string& messageId)
{
   if (messages_.contains(partyId)) {
      auto it = std::find_if(messages_[partyId].begin(), messages_[partyId].end(), [messageId](Chat::MessagePtr message) {
         return message->messageId() == messageId;
      });

      if (it != messages_[partyId].end()) {
         return *it;
      }
   }

   return {};
}

void ChatMessagesTextEdit::notifyMessageChanged(Chat::MessagePtr message)
{
   const std::string& partyId = message->partyId();
   if (partyId != currentPartyId_) {
      // Do not need to update view
      return;
   }

   if (messages_.contains(partyId)) {
      const std::string& id = message->messageId();
      auto it = std::find_if(messages_[partyId].begin(), messages_[partyId].end(), [id](Chat::MessagePtr iteration) {
         return iteration->messageId() == id;
      });

      if (it != messages_[partyId].end()) {
         int distance = static_cast<int>(std::distance(messages_[partyId].begin(), it));

         QTextCursor cursor = textCursor();
         cursor.movePosition(QTextCursor::Start);
         cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, distance * 2);
         cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 1);
         cursor.removeSelectedText();
         
         insertMessageInDoc(cursor, partyId, distance);
      }
   }
}

QString ChatMessagesTextEdit::toHtmlUsername(const QString &username, const QString &userId)
{
   QString changedUsername = QString(QLatin1Literal("<a href=\"user:%1\" style=\"color:%2\">%1</a>")).arg(username).arg(internalStyle_.colorHyperlink().name());

   if (userId.length() > 0) {
      changedUsername = QString(QLatin1Literal("<a href=\"user:%1\" style=\"color:%2\">%3</a>")).arg(userId).arg(internalStyle_.colorHyperlink().name()).arg(username);
   }

   return changedUsername;
}

QString ChatMessagesTextEdit::toHtmlInvalid(const QString &text)
{
   QString changedText = QString(QLatin1Literal("<font color=\"%1\">%2</font>")).arg(QLatin1String("red")).arg(text);
   return changedText;
}

QString ChatMessagesTextEdit::toHtmlText(const QString &text)
{
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
      QString hyperlinkText = QString(QLatin1Literal("<a href=\"%1\" style=\"color:%2\">%1</a>")).arg(linkText).arg(internalStyle_.colorHyperlink().name());

      changedText = changedText.replace(startIndex, endIndex - startIndex, hyperlinkText);

      index = startIndex + hyperlinkText.length();
   }

   // replace linefeed with <br>
   changedText.replace(QLatin1Literal("\n"), QLatin1Literal("<br>"));

   // set text color as white
   changedText = QString(QLatin1Literal("<font color=\"%1\">%2</font>")).arg(internalStyle_.colorWhite().name()).arg(changedText);

   return changedText;
}
