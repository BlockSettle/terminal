#include "ChatMessagesTextEdit.h"

#include "ChatClientDataModel.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatProtocol/ChatProtocol.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QMimeData>
#include <QScrollBar>

#include <set>

const int FIRST_FETCH_MESSAGES_SIZE = 20;

ChatMessagesTextEdit::ChatMessagesTextEdit(QWidget* parent)
   : QTextBrowser(parent), handler_(nullptr), internalStyle_(this)
{
   tableFormat.setBorder(0);
   tableFormat.setCellPadding(0);
   tableFormat.setCellSpacing(0);

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

QString ChatMessagesTextEdit::data(const int &row, const Column &column)
{
   if (messages_[currentChatId_].empty()) {
       return QString();
   }

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = messages_[currentChatId_][row]->dateTime().toLocalTime();
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");
         QString sender = messages_[currentChatId_][row]->senderId();

         if (sender == ownUserId_) {
            return ownSender;
         }

         auto contactItem = client_->getDataModel()->findContactItem(sender.toStdString());
         if (contactItem == nullptr) {
            if (isGroupRoom_) {
               return toHtmlUsername(sender);
            }
            return sender;
         }

         if (contactItem->hasDisplayName()) {
            if (isGroupRoom_) {
               return toHtmlUsername(contactItem->getDisplayName(), sender);
            }
            return contactItem->getDisplayName();
         }

         return sender;
      }
      case Column::Status:{
         std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][row];
         if (message->senderId() != ownUserId_){
            if (!(message->state() & static_cast<int>(Chat::MessageData::State::Read))){
               emit MessageRead(message);
            }
            return QString();

         }
         int state = message->state();
         QString status = QLatin1String("Sending");

         if (state & static_cast<int>(Chat::MessageData::State::Sent)){
            status = QLatin1String("Sent");
         }

         if (state & static_cast<int>(Chat::MessageData::State::Acknowledged)){
            status = QLatin1String("Delivered");
         }

         if (state & static_cast<int>(Chat::MessageData::State::Read)){
            status = QLatin1String("Read");
         }
         return status;
      }

      case Column::Message: {
         std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][row];
         QString text = QLatin1String("[%1] %2");
         text = text.arg(messages_[currentChatId_][row]->id());

         if (message->state() & static_cast<int>(Chat::MessageData::State::Invalid)) {
            return toHtmlInvalid(text.arg(QLatin1String("INVALID MESSAGE!")));
         } else if (message->encryptionType() == Chat::MessageData::EncryptionType::IES) {
            return toHtmlInvalid(text.arg(QLatin1String("IES ENCRYPTED!")));
         } else if ( message->encryptionType() == Chat::MessageData::EncryptionType::AEAD) {
            return toHtmlInvalid(text.arg(QLatin1String("AEAD ENCRYPTED!")));
         }
         return toHtmlText(messages_[currentChatId_][row]->messageData());
      }
      default:
         break;
   }

   return QString();
}

QImage ChatMessagesTextEdit::statusImage(const int &row)
{

   std::shared_ptr<Chat::MessageData> message = messages_[currentChatId_][row];
   if (message->senderId() != ownUserId_){
      return QImage();
   }
   int state = message->state();

   QImage statusImage = statusImageOffline_;

   if (state & static_cast<int>(Chat::MessageData::State::Sent)){

      if (isGroupRoom_) {
         statusImage = statusImageRead_;
      } else {
         statusImage = statusImageConnecting_;
      }

   }

   if (state & static_cast<int>(Chat::MessageData::State::Acknowledged)){
      statusImage = statusImageOnline_;
   }

   if (state & static_cast<int>(Chat::MessageData::State::Read)){
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
   QString text = textCursor_.block().text();

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
               userMenu_->addAction(userAddContactAction_);
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

void ChatMessagesTextEdit::switchToChat(const QString& chatId, bool isGroupRoom)
{
   currentChatId_ = chatId;
   isGroupRoom_ = isGroupRoom;
   messages_.clear();
   messagesToLoadMore_.clear();

   clear();
   table = NULL;

   emit userHaveNewMessageChanged(chatId, false, false);
}

void ChatMessagesTextEdit::setHandler(std::shared_ptr<ChatItemActionsHandler> handler)
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
   tableFormat.setColumnWidthConstraints(col_widths);
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
   else if (!link.toString().startsWith(QLatin1Literal("user:"))) {
      QDesktopServices::openUrl(link);
   }
}

void ChatMessagesTextEdit::insertMessage(std::shared_ptr<Chat::MessageData> msg)
{
   auto rowIdx = static_cast<int>(messages_[currentChatId_].size());
   messages_[currentChatId_].push_back(msg);

   /* add text */
   QTextCursor cursor(textCursor());
   cursor.movePosition(QTextCursor::End);
   table = cursor.insertTable(1, 4, tableFormat);

   QString time = data(rowIdx, Column::Time);
   table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

   QImage image = statusImage(rowIdx);
   table->cellAt(0, 1).firstCursorPosition().insertImage(image);

   QString user = data(rowIdx, Column::User);
   table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

   QString message = data(rowIdx, Column::Message);
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

      table = cursor.insertTable(1, 4, tableFormat);

      QString time = data(i, Column::Time);
      table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

      QImage image = statusImage(i);
      table->cellAt(0, 1).firstCursorPosition().insertImage(image);

      QString user = data(i, Column::User);
      table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

      QString message = data(i, Column::Message);
      table->cellAt(0, 3).firstCursorPosition().insertHtml(message);

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
      handler_->onActionAddToContacts(username_);
   });

   userRemoveContactAction_ = new QAction(tr("Remove from contacts"));
   userRemoveContactAction_->setStatusTip(tr("Click to remove user from contact list"));
   connect(userRemoveContactAction_, &QAction::triggered, [this](bool) {
      // TODO:
      //handler_->onActionRemoveFromContacts(username_);
   });
}

void ChatMessagesTextEdit::onSingleMessageUpdate(const std::shared_ptr<Chat::MessageData> &msg)
{
   insertMessage(msg);

   emit rowsInserted();
}

void ChatMessagesTextEdit::onMessageIdUpdate(const QString& oldId, const QString& newId, const QString& chatId)
{
   std::shared_ptr<Chat::MessageData> message = findMessage(chatId, oldId);

   if (message != nullptr){
      message->setId(newId);
      message->setFlag(Chat::MessageData::State::Sent);
      notifyMessageChanged(message);
   }
}

void ChatMessagesTextEdit::onMessageStatusChanged(const QString& messageId, const QString chatId, int newStatus)
{
   std::shared_ptr<Chat::MessageData> message = findMessage(chatId, messageId);

   if (message != nullptr){
      message->updateState(newStatus);
      notifyMessageChanged(message);
   }
}

std::shared_ptr<Chat::MessageData> ChatMessagesTextEdit::findMessage(const QString& chatId, const QString& messageId)
{
   std::shared_ptr<Chat::MessageData> found = nullptr;
   if (messages_.contains(chatId)) {
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [messageId](std::shared_ptr<Chat::MessageData> data){
         return data->id() == messageId;
      });

      if (it != messages_[chatId].end()) {
         found = *it;
      }
   }
   return found;
}

void ChatMessagesTextEdit::notifyMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   const QString chatId = message->senderId() == ownUserId_
                          ? message->receiverId()
                          : message->senderId();

   if (messages_.contains(chatId)) {
      QString id = message->id();
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [id](std::shared_ptr<Chat::MessageData> data){
         return data->id() == id;
      });

      if (it != messages_[chatId].end()) {
         int distance = static_cast<int>(std::distance(messages_[chatId].begin(), it));

         QTextCursor cursor(textCursor());
         cursor.movePosition(QTextCursor::Start);
         cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, distance * 2 + (messagesToLoadMore_.size() > 0));
         cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 2);
         cursor.removeSelectedText();

         table = cursor.insertTable(1, 4, tableFormat);

         QString time = data(distance, Column::Time);
         table->cellAt(0, 0).firstCursorPosition().insertHtml(time);

         QImage image = statusImage(distance);
         table->cellAt(0, 1).firstCursorPosition().insertImage(image);

         QString user = data(distance, Column::User);
         table->cellAt(0, 2).firstCursorPosition().insertHtml(user);

         QString message = data(distance, Column::Message);
         table->cellAt(0, 3).firstCursorPosition().insertHtml(message);

         emit rowsInserted();
      }
   }
}

void ChatMessagesTextEdit::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages, bool isFirstFetch)
{

   for (const auto& message: messages) {
      messages_[currentChatId_].push_back(message);
   }
   for (const auto& message : messages) {
      insertMessage(message);
   }
   for (const auto& message : messages) {
      if (messageReadHandler_ && !(message->state() & (int)Chat::MessageData::State::Read) ){
         messageReadHandler_->onMessageRead(message);
      }
   }
   return;

   if (isFirstFetch) {
      for (const auto &msg : messages) {
         if (msg->senderId() == currentChatId_) {
            messagesToLoadMore_.push_back(msg);
            emit MessageRead(msg);
         }
         else {
            messages_[msg->senderId()].push_back(msg);
         }
      }

      if (messagesToLoadMore_.size() > FIRST_FETCH_MESSAGES_SIZE) {
         /* display certain count of messages and thus remove the displayed messages from the messagesToLoadMore */

         // add "load more" hyperlink text
         insertLoadMore();

         // display last messages
         unsigned long i = 0;
         for (const auto &msg: messagesToLoadMore_) {
            if (i >= messagesToLoadMore_.size() - FIRST_FETCH_MESSAGES_SIZE) {
               insertMessage(msg);
            }

            i++;
         }

         // remove the messages shown
         for (i = 0; i < FIRST_FETCH_MESSAGES_SIZE; i++) {
            messagesToLoadMore_.pop_back();
         }
      } else { // flush all messages
         for (const auto &msg: messagesToLoadMore_) {
            insertMessage(msg);
         }

         messagesToLoadMore_.clear();
      }
   }
   else {
      for (const auto &msg : messages) {
         if (msg->senderId() == currentChatId_) {
            insertMessage(msg);
            if (!(msg->state() & static_cast<int>(Chat::MessageData::State::Read))){
               emit MessageRead(msg);
            }

            emit userHaveNewMessageChanged(msg->senderId(), false, true);
         }
         else {
            messages_[msg->senderId()].push_back(msg);

            emit userHaveNewMessageChanged(msg->senderId(), true, false);
         }
      }
   }

   emit rowsInserted();
}

void ChatMessagesTextEdit::onRoomMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>>& messages, bool isFirstFetch)
{
   for (const auto& message : messages) {
      messages_[currentChatId_].push_back(message);
   }
   for (const auto& message : messages) {
      insertMessage(message);
      if (messageReadHandler_ && !message->testFlag(Chat::MessageData::State::Read)){
         messageReadHandler_->onRoomMessageRead(message);
      }
   }
   return;

   if (isFirstFetch) {
      for (const auto &msg : messages) {
         if (msg->receiverId() == currentChatId_) {
            messagesToLoadMore_.push_back(msg);
         }
         else {
            messages_[msg->receiverId()].push_back(msg);
         }
      }

      if (messagesToLoadMore_.size() > FIRST_FETCH_MESSAGES_SIZE) {
         /* display certain count of messages and thus remove the displayed messages from the messagesToLoadMore */

         // add "load more" hyperlink text
         insertLoadMore();

         // display last messages
         unsigned long i = 0;
         for (const auto &msg: messagesToLoadMore_) {
            if (i >= messagesToLoadMore_.size() - FIRST_FETCH_MESSAGES_SIZE) {
               insertMessage(msg);
            }

            i++;
         }

         // remove the messages shown
         for (i = 0; i < FIRST_FETCH_MESSAGES_SIZE; i++) {
            messagesToLoadMore_.pop_back();
         }
      } else { // flush all messages
         for (const auto &msg: messagesToLoadMore_) {
            insertMessage(msg);
         }

         messagesToLoadMore_.clear();
      }
   }
   else {
      std::set<QString> receivers;
      for (const auto &msg : messages) {
         receivers.insert(msg->receiverId());
         if (msg->receiverId() == currentChatId_) {
            insertMessage(msg);
         }
         else {
            messages_[msg->receiverId()].push_back(msg);
         }
      }
      for (const QString& recv : receivers) {
         emit userHaveNewMessageChanged(recv, recv != currentChatId_, recv == currentChatId_);
      }
   }

   emit rowsInserted();
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
   if (!element){
      return;
   }

   std::vector<std::shared_ptr<Chat::MessageData>> messages;
   for (auto msg_item : element->getChildren()){
      auto item = static_cast<TreeMessageNode*>(msg_item);
      if (item){
         auto msg = item->getMessage();
         if (msg) {
            messages.push_back(msg);
         }
      }
   }

   auto data = element->getDataObject();
   switch (data->getType()) {
      case Chat::DataObject::Type::RoomData:{
         auto room = std::dynamic_pointer_cast<Chat::RoomData>(data);
         if (room) {
            switchToChat(room->getId(), true);
            onRoomMessagesUpdate(messages, true);
         }
      }
      break;
      case Chat::DataObject::Type::ContactRecordData: {
         auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
         if (contact) {
            switchToChat(contact->getContactId(), false);
            onMessagesUpdate(messages, true);
         }
      }
      break;
      default:
         return;
   }
}

void ChatMessagesTextEdit::onMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   qDebug() << __func__ << " " << QString::fromStdString(message->toJsonString());
   if (message->senderId() == currentChatId_ || message->receiverId() == currentChatId_) {
      notifyMessageChanged(message);
   }
}

void ChatMessagesTextEdit::onElementUpdated(CategoryElement *element)
{
   //TODO: Important! optimize messages reload
   auto data = element->getDataObject();
   switch (data->getType()) {
      case Chat::DataObject::Type::RoomData:{
         auto room = std::dynamic_pointer_cast<Chat::RoomData>(data);
         if (room && room->getId() == currentChatId_){
            messages_.clear();
            clear();
            std::vector<std::shared_ptr<Chat::MessageData>> messages;
            for (auto msg_item : element->getChildren()){
               auto item = static_cast<TreeMessageNode*>(msg_item);
               auto msg = item->getMessage();
               messages.push_back(msg);
            }
            onRoomMessagesUpdate(messages, true);
         }
      }
      break;
      case Chat::DataObject::Type::ContactRecordData: {
         auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
         if (contact && contact->getContactId() == currentChatId_){
            messages_.clear();
            clear();
            std::vector<std::shared_ptr<Chat::MessageData>> messages;
            for (auto msg_item : element->getChildren()){
               auto item = static_cast<TreeMessageNode*>(msg_item);
               auto msg = item->getMessage();
               messages.push_back(msg);
            }
            onMessagesUpdate(messages, true);
         }
      }
      break;
      default:
         return;
   }

}
