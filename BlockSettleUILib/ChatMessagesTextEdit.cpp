#include "ChatMessagesTextEdit.h"

#include "ChatClientDataModel.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatProtocol/ChatUtils.h"
#include "ProtobufUtils.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QMimeData>
#include <QScrollBar>

#include <set>

ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextBrowser(parent), handler_(nullptr), internalStyle_(this)
{
   tableFormat_.setBorder(0);
   tableFormat_.setCellPadding(0);
   tableFormat_.setCellSpacing(0);

   setupHighlightPalette();

   setAlignment(Qt::AlignHCenter);
   setAutoFormatting(QTextEdit::AutoAll);
   setAcceptRichText(true);
   setOpenExternalLinks(false);
   setOpenLinks(false);

   statusImageOffline_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_OFFLINE"), "PNG");
   statusImageConnecting_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_CONNECTING"), "PNG");
   statusImageOnline_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_ONLINE"), "PNG");
   statusImageRead_ = QImage(QLatin1Literal(":/ICON_MSG_STATUS_READ"), "PNG");

   connect(this, &QTextBrowser::anchorClicked, this, &ChatMessagesTextEdit::urlActivated);
   connect(this, &QTextBrowser::textChanged, this, &ChatMessagesTextEdit::onTextChanged);

   initUserContextMenu();
}

QString ChatMessagesTextEdit::data(int row, const Column &column)
{
   if (messages_[currentChatId_].empty()) {
       return QString();
   }

   const auto &message = messages_[currentChatId_][row];
   // PK: Not sure if otc_case check is needed here, but before protobuf switch exact type checked here
   // (message might optionally have OTC details)
   if (message->has_message() && message->message().otc_case() == Chat::Data_Message::OTC_NOT_SET) {
      return dataMessage(row, column);
   }

   return QLatin1String("[unk]");
}

QString ChatMessagesTextEdit::dataMessage(int row, const ChatMessagesTextEdit::Column &column)
{
   const auto data = messages_[currentChatId_][row];
   const auto &message = data->message();

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = QDateTime::fromMSecsSinceEpoch(message.timestamp_ms()).toLocalTime();
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");

         if (message.sender_id() == ownUserId_) {
            return ownSender;
         }

         auto contactItem = client_->getDataModel()->findContactItem(message.sender_id());
         if (contactItem == nullptr) {
            if (isGroupRoom_) {
               return toHtmlUsername(QString::fromStdString(message.sender_id()));
            }
            return QString::fromStdString(message.sender_id());
         }

         if (!contactItem->contact_record().display_name().empty()) {
            if (isGroupRoom_) {
               return toHtmlUsername(QString::fromStdString(contactItem->contact_record().display_name())
                  , QString::fromStdString(message.sender_id()));
            }
            return QString::fromStdString(contactItem->contact_record().display_name());
         }

         return QString::fromStdString(message.sender_id());
      }
      case Column::Status:{
         if (message.sender_id() != ownUserId_) {
            if (!ChatUtils::messageFlagRead(message, Chat::Data_Message_State_READ)) {
               emit MessageRead(data);
            }
            return QString();

         }
         QString status = QLatin1String("Sending");

         if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_SENT)) {
            status = QLatin1String("Sent");
         }

         if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_ACKNOWLEDGED)) {
            status = QLatin1String("Delivered");
         }

         if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_READ)) {
            status = QLatin1String("Read");
         }
         return status;
      }

      case Column::Message: {
         QString text = QLatin1String("[%1] %2");
         text = text.arg(QString::fromStdString(message.id()));

         if (ChatUtils::messageFlagRead(message, Chat::Data_Message_State_INVALID)) {
            return toHtmlInvalid(text.arg(QLatin1String("INVALID MESSAGE!")));
         } else if (message.encryption() == Chat::Data_Message_Encryption_IES) {
            return toHtmlInvalid(text.arg(QLatin1String("IES ENCRYPTED!")));
         } else if ( message.encryption() == Chat::Data_Message_Encryption_AEAD) {
            return toHtmlInvalid(text.arg(QLatin1String("AEAD ENCRYPTED!")));
         }
         return toHtmlText(QString::fromStdString(message.message()));
      }
      default:
         break;
   }

   return QString();
}

QImage ChatMessagesTextEdit::statusImage(int row)
{
   auto data = messages_[currentChatId_][row];
   if (!data->has_message()) {
      return statusImageConnecting_;
   }


   std::shared_ptr<Chat::Data> message = messages_[currentChatId_][row];
   if (message->message().sender_id() != ownUserId_) {
      return QImage();
   }

   QImage statusImage = statusImageOffline_;

   if (ChatUtils::messageFlagRead(data->message(), Chat::Data_Message_State_SENT)) {

      if (isGroupRoom_) {
         statusImage = statusImageRead_;
      } else {
         statusImage = statusImageConnecting_;
      }

   }

   if (ChatUtils::messageFlagRead(data->message(), Chat::Data_Message_State_ACKNOWLEDGED)) {
      statusImage = statusImageOnline_;
   }

   if (ChatUtils::messageFlagRead(data->message(), Chat::Data_Message_State_READ)) {
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

void ChatMessagesTextEdit::switchToChat(const std::string& chatId, bool isGroupRoom)
{
   currentChatId_ = chatId;
   isGroupRoom_ = isGroupRoom;
   messages_.clear();
   messagesToLoadMore_.clear();

   clear();
   table_ = nullptr;

   emit userHaveNewMessageChanged(chatId, false, false);
}

void ChatMessagesTextEdit::setHandler(ChatItemActionsHandler* handler)
{
   handler_ = handler;
}

void ChatMessagesTextEdit::setMessageReadHandler(std::shared_ptr<ChatMessageReadHandler> handler)
{
   messageReadHandler_ = handler;
}

void ChatMessagesTextEdit::setClient(std::shared_ptr<ChatClient> client)
{
   client_ = client;
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

void ChatMessagesTextEdit::setIsChatTab(const bool &isChatTab)
{
   isChatTab_ = isChatTab;
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

void ChatMessagesTextEdit::insertMessage(std::shared_ptr<Chat::Data> msg)
{
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   messages_[currentChatId_].push_back(msg);

   /* add text */
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::End);
   table_ = cursor.insertTable(1, 4, tableFormat_);

   QString time = data(rowIdx, Column::Time);
   table_->cellAt(0, 0).firstCursorPosition().insertHtml(time);

   QImage image = statusImage(rowIdx);
   table_->cellAt(0, 1).firstCursorPosition().insertImage(image);

   QString user = data(rowIdx, Column::User);
   table_->cellAt(0, 2).firstCursorPosition().insertHtml(user);

   QString message = data(rowIdx, Column::Message);
   table_->cellAt(0, 3).firstCursorPosition().insertHtml(message);
}

void ChatMessagesTextEdit::insertLoadMore()
{
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::Start);
   cursor.insertHtml(QString(QLatin1Literal("<a href=\"load_more\" style=\"color:%1\">Load More...</a>")).arg(internalStyle_.colorHyperlink().name()));
}

void ChatMessagesTextEdit::loadMore()
{

   // delete insert more button
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::Start);
   cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor, 1);
   cursor.removeSelectedText();

   // load more messages
   int i = 0;
   for (const auto &msg: messagesToLoadMore_) {
      cursor.movePosition(QTextCursor::Start);
      cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, i * 2);

      messages_[currentChatId_].insert(messages_[currentChatId_].begin() + i, msg);

      table_ = cursor.insertTable(1, 4, tableFormat_);

      QString time = data(i, Column::Time);
      table_->cellAt(0, 0).firstCursorPosition().insertHtml(time);

      QImage image = statusImage(i);
      table_->cellAt(0, 1).firstCursorPosition().insertImage(image);

      QString user = data(i, Column::User);
      table_->cellAt(0, 2).firstCursorPosition().insertHtml(user);

      QString message = data(i, Column::Message);
      table_->cellAt(0, 3).firstCursorPosition().insertHtml(message);

      i++;
   }

   messagesToLoadMore_.clear();
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

void ChatMessagesTextEdit::onSingleMessageUpdate(const std::shared_ptr<Chat::Data> &msg)
{
   insertMessage(msg);

   emit rowsInserted();
}

void ChatMessagesTextEdit::onMessageIdUpdate(const std::string& oldId, const std::string& newId, const std::string& chatId)
{
   std::shared_ptr<Chat::Data> message = findMessage(chatId, oldId);

   if (message) {
      message->mutable_message()->set_id(newId);
      ChatUtils::messageFlagSet(message->mutable_message(), Chat::Data_Message_State_SENT);
      notifyMessageChanged(message);
   }
}

void ChatMessagesTextEdit::onMessageStatusChanged(const std::string& messageId, const std::string &chatId, int newStatus)
{
   std::shared_ptr<Chat::Data> message = findMessage(chatId, messageId);

   if (message) {
      message->mutable_message()->set_state(newStatus);
      notifyMessageChanged(message);
   }
}

std::shared_ptr<Chat::Data> ChatMessagesTextEdit::findMessage(const std::string& chatId, const std::string& messageId)
{
   std::shared_ptr<Chat::Data> found = nullptr;
   if (messages_.contains(chatId)) {
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [messageId](std::shared_ptr<Chat::Data> data) {
         return data->has_message() && data->message().id() == messageId;
      });

      if (it != messages_[chatId].end()) {
         found = *it;
      }
   }
   return found;
}

void ChatMessagesTextEdit::notifyMessageChanged(std::shared_ptr<Chat::Data> message)
{
   const std::string chatId = message->message().sender_id() == ownUserId_
                          ? message->message().receiver_id()
                          : message->message().sender_id();

   if (messages_.contains(chatId)) {
      const std::string &id = message->message().id();
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [id](std::shared_ptr<Chat::Data> data) {
         return data->has_message() && data->message().id() == id;
      });

      if (it != messages_[chatId].end()) {
         int distance = static_cast<int>(std::distance(messages_[chatId].begin(), it));

         QTextCursor cursor(textCursor());
         cursor.movePosition(QTextCursor::Start);
         cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, distance * 2 + (messagesToLoadMore_.size() > 0));
         cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 2);
         cursor.removeSelectedText();

         table_ = cursor.insertTable(1, 4, tableFormat_);

         QString time = data(distance, Column::Time);
         table_->cellAt(0, 0).firstCursorPosition().insertHtml(time);

         QImage image = statusImage(distance);
         table_->cellAt(0, 1).firstCursorPosition().insertImage(image);

         QString user = data(distance, Column::User);
         table_->cellAt(0, 2).firstCursorPosition().insertHtml(user);

         QString message = data(distance, Column::Message);
         table_->cellAt(0, 3).firstCursorPosition().insertHtml(message);

         emit rowsInserted();
      }
   }
}

void ChatMessagesTextEdit::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::Data>>& messages, bool isFirstFetch)
{
//   for (const auto& message: messages) {
//      messages_[currentChatId_].push_back(message);
//   }
   for (const auto& message : messages) {
      insertMessage(message);
   }
   if (isChatTab_ && QApplication::activeWindow()) {
      for (const auto& data : messages) {
         if (data->has_message()) {
            if (messageReadHandler_
                && !ChatUtils::messageFlagRead(data->message(), Chat::Data_Message_State_READ))
            {
               messageReadHandler_->onMessageRead(data);
            }
         }
      }
   }
}

void ChatMessagesTextEdit::onRoomMessagesUpdate(const std::vector<std::shared_ptr<Chat::Data>>& messages, bool isFirstFetch)
{
   for (const auto& message : messages) {
      insertMessage(message);
   }
   if (isChatTab_ && QApplication::activeWindow()) {
      for (const auto& data : messages) {
         if (data->has_message()) {
            if (messageReadHandler_
                && !(ChatUtils::messageFlagRead(data->message(), Chat::Data_Message_State_READ)))
            {
               messageReadHandler_->onMessageRead(data);
            }
         }
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

void ChatMessagesTextEdit::onElementSelected(CategoryElement *element)
{
   if (!element || !element->getDataObject()) {
      return;
   }

   std::vector<std::shared_ptr<Chat::Data>> displayData;
   bool messageOnly = true;
   for (auto msg_item : element->getChildren()) {
      auto item = dynamic_cast<DisplayableDataNode*>(msg_item);
      if (item) {
         if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::MessageDataNode) {
            auto data = item->getDataObject();
            if (data->has_message()) {
               displayData.push_back(data);
            }
         } else {
            messageOnly = false;
            auto msg = item->getDataObject();
            if (msg) {
               displayData.push_back(msg);
            }
         }
      }
   }

   auto data = element->getDataObject();
   if (data->has_room()) {
      switchToChat(data->room().id(), true);
      onRoomMessagesUpdate(displayData, true);
   }

   if (data->has_contact_record()) {
      switchToChat(data->contact_record().contact_id(), false);

      onMessagesUpdate(displayData, true);
   }
}

void ChatMessagesTextEdit::onMessageChanged(std::shared_ptr<Chat::Data> message)
{
   //qDebug() << __func__ << " " << QString::fromStdString(message->toJsonString());

   if (message->message().sender_id() == currentChatId_ || message->message().receiver_id() == currentChatId_) {
      notifyMessageChanged(message);
   }
}

void ChatMessagesTextEdit::onElementUpdated(CategoryElement *element)
{
   //TODO: Important! optimize messages reload
   auto data = element->getDataObject();

   if (!data) {
      return;
   }

   if (data->has_room() && data->room().id() == currentChatId_) {
      messages_.clear();
      clear();
      std::vector<std::shared_ptr<Chat::Data>> displayData;
      for (auto msg_item : element->getChildren()) {
         auto item = dynamic_cast<DisplayableDataNode*>(msg_item);
         auto msg = item->getDataObject();
         displayData.push_back(msg);
      }
      onRoomMessagesUpdate(displayData, true);
   }

   if (data->has_contact_record() && data->contact_record().contact_id() == currentChatId_) {
      messages_.clear();
      clear();
      std::vector<std::shared_ptr<Chat::Data>> displayData;
      for (auto msg_item : element->getChildren()) {
         auto item = dynamic_cast<DisplayableDataNode*>(msg_item);
         auto msg = item->getDataObject();
         displayData.push_back(msg);
      }
      onMessagesUpdate(displayData, true);
   }
}

void ChatMessagesTextEdit::onCurrentElementAboutToBeRemoved()
{
   messages_.clear();
   messagesToLoadMore_.clear();

   clear();
   table_ = nullptr;
}
