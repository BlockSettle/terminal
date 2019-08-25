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
   bool isGlobal(Chat::ClientPartyPtr clientPartyPtr) {
      return clientPartyPtr->partyType() == Chat::PartyType::GLOBAL;
   }

   bool isPrivate(Chat::ClientPartyPtr clientPartyPtr) {
      return clientPartyPtr->partyType() == Chat::PartyType::GLOBAL;
   }
}

ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextBrowser(parent), handler_(nullptr), internalStyle_(this)
{
   tableFormat_.setBorder(0);
   tableFormat_.setCellPadding(0);
   tableFormat_.setCellSpacing(0);

   setupHighlightPalette();

   connect(this, &QTextBrowser::anchorClicked, this, &ChatMessagesTextEdit::urlActivated);
   connect(this, &QTextBrowser::textChanged, this, &ChatMessagesTextEdit::onTextChanged);

   initUserContextMenu();
}

QString ChatMessagesTextEdit::data(int row, const Column &column)
{
   if (messages_[currentChatId_].empty()) {
       return QString();
   }

   // TODO: Filter OTC messages
   return dataMessage(row, column);
}

QString ChatMessagesTextEdit::dataMessage(int row, const ChatMessagesTextEdit::Column &column)
{
   const Chat::MessagePtr message = messages_[currentChatId_][row];

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = QDateTime::fromMSecsSinceEpoch(message->timestamp()).toLocalTime();
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         const auto senderId = message->sender();
         static const auto ownSender = tr("you");

         if (senderId == ownUserId_) {
            return ownSender;
         }

         Chat::ClientPartyPtr clientPartyPtr = partyModel_->getClientPartyById(senderId);
         if (!clientPartyPtr) {
            return QString::fromStdString(senderId);
         }

         QString partyName;
         if (clientPartyPtr->displayName().empty()) {
            partyName = QString::fromStdString(senderId);
         }
         else {
            partyName = QString::fromStdString(clientPartyPtr->displayName());
         }

         if (isGlobal(clientPartyPtr)) {
            partyName = toHtmlUsername(partyName);
         }

         return QString::fromStdString(clientPartyPtr->displayName());
      }
      case Column::Status:{
         //message->partyMessageState();

         //if (message.sender_id() != ownUserId_) {
         //   if (!ChatUtils::messageFlagRead(message, Chat::Data_Message_State_READ)) {
         //      emit MessageRead(data);
         //   }
         //   return QString();

         //}
         //QString status = QLatin1String("Sending");

         //if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_SENT)) {
         //   status = QLatin1String("Sent");
         //}

         //if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_ACKNOWLEDGED)) {
         //   status = QLatin1String("Delivered");
         //}

         //if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_READ)) {
         //   status = QLatin1String("Read");
         //}
         //return status;
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

QImage ChatMessagesTextEdit::statusImage(int row)
{
   auto message = messages_[currentChatId_][row];
   if (!message) {
      return statusImageConnecting_;
   }

   auto senderId = message->sender();
   if (message->sender() != ownUserId_) {
      return QImage();
   }

   QImage statusImage = statusImageOffline_;

   if (message->partyMessageState() == Chat::PartyMessageState::SENT) {
      Chat::ClientPartyPtr clientPartyPtr = partyModel_->getClientPartyById(senderId);

      if (isGlobal(clientPartyPtr)) {
         statusImage = statusImageRead_;
      } else {
         statusImage = statusImageConnecting_;
      }
   }
   else if (message->partyMessageState() == Chat::PartyMessageState::UNSENT) {
      statusImage = statusImageOnline_;
   } else if (message->partyMessageState() == Chat::PartyMessageState::SEEN) {
      statusImage = statusImageRead_;
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

   // show contact context menu when username is right clicked in User column
   if ((textCursor_.block().blockNumber() - 1) % 5 == static_cast<int>(Column::User) ) {
      if (!anchorAt(e->pos()).isEmpty()) {
         username_ = text;

         if (handler_) {
            for (auto action : userMenu_->actions()) {
               userMenu_->removeAction(action);
            }
            if (handler_->onActionIsFriend(username_)) {
               userMenu_->addAction(userRemoveContactAction_);
            }
            else {
               emit addContactRequired(QString::fromStdString(username_));
            }
            userMenu_->exec(QCursor::pos());
         }
         return;
      }
   }

   // show default text context menu
   if (text.length() > 0 || textCursor_.hasSelection()) {
      QMenu contextMenu(this);

      QAction copyAction(tr("Copy"), this);
      QAction copyLinkLocationAction(tr("Copy Link Location"), this);
      QAction selectAllAction(tr("Select All"), this);

      connect(&copyAction, SIGNAL(triggered()), this, SLOT(copyActionTriggered()));
      connect(&copyLinkLocationAction, SIGNAL(triggered()), this, SLOT(copyLinkLocationActionTriggered()));
      connect(&selectAllAction, SIGNAL(triggered()), this, SLOT(selectAllActionTriggered()));

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
   username_ = url.path().toStdString();
   if (!handler_->onActionIsFriend(username_)) {
      emit addContactRequired(QString::fromStdString(username_));
   }
}

void ChatMessagesTextEdit::switchToChat(const std::string& chatId)
{
   currentChatId_ = chatId;
   forceMessagesUpdate();
}

void ChatMessagesTextEdit::resetChatView()
{
   switchToChat({});
}

void ChatMessagesTextEdit::logout()
{
   resetChatView();
   messages_.clear();
}

void ChatMessagesTextEdit::onMessageStatusChanged(const std::string& partyId, const std::string& message_id, const int party_message_state)
{
   Chat::MessagePtr message = findMessage(partyId, message_id);

   if (message) {
      message->setPartyMessageState(static_cast<Chat::PartyMessageState>(party_message_state));
      notifyMessageChanged(message);
   }
}

void ChatMessagesTextEdit::setHandler(ChatItemActionsHandler* handler)
{
   handler_ = handler;
}

void ChatMessagesTextEdit::setMessageReadHandler(std::shared_ptr<ChatMessageReadHandler> handler)
{
   messageReadHandler_ = handler;
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
   if (link.toString() == QLatin1Literal("load_more")) {
      loadMore();
   }
   else if (link.scheme() != QLatin1Literal("user")) {
      QDesktopServices::openUrl(link);
   } else {
      onUserUrlOpened(link);
   }
}

void ChatMessagesTextEdit::insertMessage(const Chat::MessagePtr& messagePtr)
{
   const int messageIndex = messages_[messagePtr->partyId()].size();
   messages_[messagePtr->partyId()].push_back(messagePtr);
   if (messagePtr->partyId() == currentChatId_) {
      showMessage(messageIndex);
   }
}

void ChatMessagesTextEdit::showMessage(int messageIndex)
{
   /* add text */
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::End);
   auto* table = cursor.insertTable(1, 4, tableFormat_);

   QString time = data(messageIndex, Column::Time);
   table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

   QImage image = statusImage(messageIndex);
   if (!image.isNull()) {
      table->cellAt(0, 1).firstCursorPosition().insertImage(image);
   }

   QString user = data(messageIndex, Column::User);
   table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

   QString message = data(messageIndex, Column::Message);
   table->cellAt(0, 3).firstCursorPosition().insertHtml(message);
}

void ChatMessagesTextEdit::insertLoadMore()
{
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::Start);
   cursor.insertHtml(QString(QLatin1Literal("<a href=\"load_more\" style=\"color:%1\">Load More...</a>")).arg(internalStyle_.colorHyperlink().name()));
}

void ChatMessagesTextEdit::loadMore()
{
   // #old_logic
   //// delete insert more button
   //QTextCursor cursor(textCursor());
   //cursor.movePosition(QTextCursor::Start);
   //cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor, 1);
   //cursor.removeSelectedText();

   //// load more messages
   //int i = 0;
   //for (const auto &msg: messagesToLoadMore_) {
   //   cursor.movePosition(QTextCursor::Start);
   //   cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, i * 2);

   //   messages_[currentChatId_].insert(messages_[currentChatId_].begin() + i, msg);

   //   table_ = cursor.insertTable(1, 4, tableFormat_);

   //   QString time = data(i, Column::Time);
   //   table_->cellAt(0, 0).firstCursorPosition().insertHtml(time);

   //   QImage image = statusImage(i);
   //   table_->cellAt(0, 1).firstCursorPosition().insertImage(image);

   //   QString user = data(i, Column::User);
   //   table_->cellAt(0, 2).firstCursorPosition().insertHtml(user);

   //   QString message = data(i, Column::Message);
   //   table_->cellAt(0, 3).firstCursorPosition().insertHtml(message);

   //   i++;
   //}

   //messagesToLoadMore_.clear();
}

void ChatMessagesTextEdit::setupHighlightPalette()
{
   auto highlightPalette = palette();
   highlightPalette.setColor(QPalette::Inactive, QPalette::Highlight, highlightPalette.color(QPalette::Active, QPalette::Highlight));
   highlightPalette.setColor(QPalette::Inactive, QPalette::HighlightedText, highlightPalette.color(QPalette::Active, QPalette::HighlightedText));
   setPalette(highlightPalette);   
}

void ChatMessagesTextEdit::initUserContextMenu()
{
   userMenu_ = new QMenu(this);

   userAddContactAction_ = new QAction(tr("Add to contacts"));
   userAddContactAction_->setStatusTip(tr("Click to add user to contact list"));
   connect(userAddContactAction_, &QAction::triggered, [this](bool) {
      handler_->onActionCreatePendingOutgoing(username_);
   });

   userRemoveContactAction_ = new QAction(tr("Remove from contacts"));
   userRemoveContactAction_->setStatusTip(tr("Click to remove user from contact list"));
   connect(userRemoveContactAction_, &QAction::triggered, [this](bool) {
      // TODO:
      //handler_->onActionRemoveFromContacts(username_);
   });
}

void ChatMessagesTextEdit::onSingleMessageUpdate(const Chat::MessagePtrList& messagePtrList)
{
   for (const auto& messagePtr : messagePtrList)
   {
      insertMessage(messagePtr);
   }
}

void ChatMessagesTextEdit::onMessageIdUpdate(const std::string& oldId, const std::string& newId, const std::string& chatId)
{
   // #old_logic
   //std::shared_ptr<Chat::Data> message = findMessage(chatId, oldId);

   //if (message) {
   //   message->mutable_message()->set_id(newId);
   //   ChatUtils::messageFlagSet(message->mutable_message(), Chat::Data_Message_State_SENT);
   //   notifyMessageChanged(message);
   //}
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
   // #old_logic
   const std::string& partyId = message->partyId();
   if (messages_.contains(partyId)) {
      const std::string& id = message->messageId();
      auto it = std::find_if(messages_[partyId].begin(), messages_[partyId].end(), [id](Chat::MessagePtr message) {
         return message->messageId() == id;
      });

      if (it != messages_[partyId].end()) {
         int distance = static_cast<int>(std::distance(messages_[partyId].begin(), it));

         QTextCursor cursor(textCursor());
         cursor.movePosition(QTextCursor::Start);
         cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, distance * 2);
         cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 2);
         cursor.removeSelectedText();

         auto* table = cursor.insertTable(1, 4, tableFormat_);

         QString time = data(distance, Column::Time);
         table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

         QImage image = statusImage(distance);
         table->cellAt(0, 1).firstCursorPosition().insertImage(image);

         QString user = data(distance, Column::User);
         table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

         QString message = data(distance, Column::Message);
         table->cellAt(0, 3).firstCursorPosition().insertHtml(message);
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

// #old_logic

//void ChatMessagesTextEdit::onElementSelected(CategoryElement *element)
//{
//   if (!element || !element->getDataObject()) {
//      return;
//   }
//
//   std::vector<std::shared_ptr<Chat::Data>> displayData;
//   bool messageOnly = true;
//   for (auto msg_item : element->getChildren()) {
//      auto item = dynamic_cast<DisplayableDataNode*>(msg_item);
//      if (item) {
//         if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::MessageDataNode) {
//            auto data = item->getDataObject();
//            if (data->has_message()) {
//               displayData.push_back(data);
//            }
//         } else {
//            messageOnly = false;
//            auto msg = item->getDataObject();
//            if (msg) {
//               displayData.push_back(msg);
//            }
//         }
//      }
//   }
//
//   auto data = element->getDataObject();
//   if (data->has_room()) {
//      switchToChat(data->room().id(), true);
//      onRoomMessagesUpdate(displayData, true);
//   }
//
//   if (data->has_contact_record()) {
//      switchToChat(data->contact_record().contact_id(), false);
//
//      onMessagesUpdate(displayData, true);
//   }
//}
//
//void ChatMessagesTextEdit::onMessageChanged(std::shared_ptr<Chat::Data> message)
//{
//   //qDebug() << __func__ << " " << QString::fromStdString(message->toJsonString());
//
//   if (message->message().sender_id() == currentChatId_ || message->message().receiver_id() == currentChatId_) {
//      notifyMessageChanged(message);
//   }
//}

void ChatMessagesTextEdit::forceMessagesUpdate()
{
   clear();

   // Nothing to do
   if (currentChatId_.empty()) {
      return;
   }

   const auto iMessages = messages_.find(currentChatId_);
   if (iMessages == messages_.end()) {
      messages_.insert(currentChatId_, {});
      return;
   }

   const auto& currentMessages = iMessages.value();
   for (int index = 0; index < currentMessages.size(); ++index) {
      const auto message = currentMessages[index];
      message->partyMessageState();
      if (message->partyMessageState() == Chat::PartyMessageState::RECEIVED && message->sender() != ownUserId_) {
         emit messageRead(message->partyId(), message->messageId());
      }
      showMessage(index);
   }
}

void ChatMessagesTextEdit::onCurrentElementAboutToBeRemoved()
{
   messages_[currentChatId_].clear();
   clear();
}
