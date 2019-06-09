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

   switch(messages_[currentChatId_][row]->getType()) {
      case Chat::DataObject::Type::MessageData:
         return dataMessage(row, column);
      case Chat::DataObject::Type::OTCRequestData:
         return dataOtcRequest(row, column);
      case Chat::DataObject::Type::OTCResponseData:
         return dataOtcResponse(row, column);
      default:
         return QLatin1String("[unk]");
   }
}

QString ChatMessagesTextEdit::dataMessage(const int &row, const ChatMessagesTextEdit::Column &column)
{
   std::shared_ptr<Chat::MessageData> message =
         std::dynamic_pointer_cast<Chat::MessageData>(messages_[currentChatId_][row]);
   switch (column) {
      case Column::Time:
      {
         const auto dateTime = message->dateTime().toLocalTime();
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");
         QString sender = message->senderId();

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
         QString text = QLatin1String("[%1] %2");
         text = text.arg(message->id());

         if (message->state() & static_cast<int>(Chat::MessageData::State::Invalid)) {
            return toHtmlInvalid(text.arg(QLatin1String("INVALID MESSAGE!")));
         } else if (message->encryptionType() == Chat::MessageData::EncryptionType::IES) {
            return toHtmlInvalid(text.arg(QLatin1String("IES ENCRYPTED!")));
         } else if ( message->encryptionType() == Chat::MessageData::EncryptionType::AEAD) {
            return toHtmlInvalid(text.arg(QLatin1String("AEAD ENCRYPTED!")));
         }
         return toHtmlText(message->messageData());
      }
      default:
         break;
   }

   return QString();
}

QString ChatMessagesTextEdit::dataOtcRequest(const int &row, const ChatMessagesTextEdit::Column &column)
{
   std::shared_ptr<Chat::OTCRequestData> otc =
         std::dynamic_pointer_cast<Chat::OTCRequestData>(messages_[currentChatId_][row]);

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = QDateTime::fromMSecsSinceEpoch(otc->submitTimestamp());
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");
         QString sender = QString::fromStdString(otc->requestorId());

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

         if (otc->requestorId() != ownUserId_.toStdString()){
//            if (!(otc->state() & static_cast<int>(Chat::MessageData::State::Read))){
//               //emit MessageRead(message);
//            }
            return QLatin1String("out");

         }

         return QLatin1String("in");
         /*
         int state = otc->state();
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
         */
      }

      case Column::Message: {

         QString text = QLatin1String("[%1] %2");
         text = text.arg(QString::fromStdString(otc->serverRequestId()).chopped(5).append(QLatin1String("...")));

         QString display;

         if(otc->requestorId() == ownUserId_.toStdString()){
            display.append(QLatin1String("REQUEST SUBMITTED: "));
         } else {
            display.append(QLatin1String("REQUEST RECEIVED: "));
         }
         display.append(QString::fromStdString(bs::network::ChatOTCSide::toString(otc->otcRequest().side)));
         display.append(QLatin1String(" [Amount: "));
         display.append(QString::fromStdString(bs::network::OTCRangeID::toString(otc->otcRequest().amountRange)));
         display.append(QLatin1String("]"));

//         if (otc->state() & static_cast<int>(Chat::MessageData::State::Invalid)) {
//            return toHtmlInvalid(text.arg(QLatin1String("INVALID MESSAGE!")));
//         } else if (otc->encryptionType() == Chat::MessageData::EncryptionType::IES) {
//            return toHtmlInvalid(text.arg(QLatin1String("IES ENCRYPTED!")));
//         } else if ( otc->encryptionType() == Chat::MessageData::EncryptionType::AEAD) {
//            return toHtmlInvalid(text.arg(QLatin1String("AEAD ENCRYPTED!")));
//         }
         return toHtmlText(display);
      }
      default:
         break;
   }

   return QString();
}

QString ChatMessagesTextEdit::dataOtcResponse(const int &row, const ChatMessagesTextEdit::Column &column)
{
   std::shared_ptr<Chat::OTCResponseData> otc =
         std::dynamic_pointer_cast<Chat::OTCResponseData>(messages_[currentChatId_][row]);

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = QDateTime::fromMSecsSinceEpoch(otc->responseTimestamp());
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");
         QString sender = QString::fromStdString(otc->responderId());

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

         if (otc->responderId() != ownUserId_.toStdString()){
//            if (!(otc->state() & static_cast<int>(Chat::MessageData::State::Read))){
//               //emit MessageRead(message);
//            }
            return QLatin1String("out");

         }

         return QLatin1String("in");
         /*
         int state = otc->state();
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
         */
      }

      case Column::Message: {

         QString text = QLatin1String("[%1] %2");
         text = text.arg(QString::fromStdString(otc->serverResponseId()).chopped(5).append(QLatin1String("...")));

         QString display;

         if(otc->responderId() == ownUserId_.toStdString()){
            display.append(QLatin1String("BID SUBMITTED: "));
         } else {
            display.append(QLatin1String("BID RECEIVED: "));
         }

         display.append(QString::number(otc->priceRange().lower));
         display.append(QLatin1String("-"));
         display.append(QString::number(otc->priceRange().upper));
         display.append(QLatin1String(" EUR @ 1 XBT "));

         display.append(QLatin1String("[Quantity: "));
         display.append(QString::number(otc->quantityRange().lower));
         display.append(QLatin1String("-"));
         display.append(QString::number(otc->quantityRange().upper));
         display.append(QLatin1String("]"));

//         if (otc->state() & static_cast<int>(Chat::MessageData::State::Invalid)) {
//            return toHtmlInvalid(text.arg(QLatin1String("INVALID MESSAGE!")));
//         } else if (otc->encryptionType() == Chat::MessageData::EncryptionType::IES) {
//            return toHtmlInvalid(text.arg(QLatin1String("IES ENCRYPTED!")));
//         } else if ( otc->encryptionType() == Chat::MessageData::EncryptionType::AEAD) {
//            return toHtmlInvalid(text.arg(QLatin1String("AEAD ENCRYPTED!")));
//         }
         return toHtmlText(display);
      }
      default:
         break;
   }

   return QString();
}

QString ChatMessagesTextEdit::dataOtcUpdate(const int &row, const ChatMessagesTextEdit::Column &column)
{
   std::shared_ptr<Chat::OTCUpdateData> otc =
         std::dynamic_pointer_cast<Chat::OTCUpdateData>(messages_[currentChatId_][row]);

   switch (column) {
      case Column::Time:
      {
         const auto dateTime = QDateTime::fromMSecsSinceEpoch(otc->updateTimestamp());
         return toHtmlText(dateTime.toString(QString::fromUtf8("MM/dd/yy hh:mm:ss")));
      }

      case Column::User:
      {
         static const auto ownSender = tr("you");
         return  ownSender;
         QString sender;// = QString::fromStdString(otc->responderId());

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

//         if (otc->responderId() != ownUserId_.toStdString()){
////            if (!(otc->state() & static_cast<int>(Chat::MessageData::State::Read))){
////               //emit MessageRead(message);
////            }
//            return QString();

//         }

         return QLatin1String("update");
         /*
         int state = otc->state();
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
         */
      }

      case Column::Message: {

         QString text = QLatin1String("[%1] %2");
         //text = text.arg(QString::fromStdString().chopped(5).append(QLatin1String("...")));

         QString display;

//         if(otc->responderId() == ownUserId_.toStdString()){
//            display.append(QLatin1String("UPDATE RECEIVED: "));
//         } else {
//            display.append(QLatin1String("BID RECEIVED: "));
//         }

//         display.append(QString::number(otc->priceRange().lower));
//         display.append(QLatin1String("-"));
//         display.append(QString::number(otc->priceRange().upper));
//         display.append(QLatin1String(" EUR @ 1 XBT "));

//         display.append(QLatin1String("[Quantity: "));
//         display.append(QString::number(otc->quantityRange().lower));
//         display.append(QLatin1String("-"));
//         display.append(QString::number(otc->quantityRange().upper));
//         display.append(QLatin1String("]"));

//         if (otc->state() & static_cast<int>(Chat::MessageData::State::Invalid)) {
//            return toHtmlInvalid(text.arg(QLatin1String("INVALID MESSAGE!")));
//         } else if (otc->encryptionType() == Chat::MessageData::EncryptionType::IES) {
//            return toHtmlInvalid(text.arg(QLatin1String("IES ENCRYPTED!")));
//         } else if ( otc->encryptionType() == Chat::MessageData::EncryptionType::AEAD) {
//            return toHtmlInvalid(text.arg(QLatin1String("AEAD ENCRYPTED!")));
//         }
         display.append(QLatin1String("UPDATE RECEIVED"));
         return toHtmlText(display);
      }
      default:
         break;
   }

   return QString();
}

QImage ChatMessagesTextEdit::statusImage(const int &row)
{
   if (messages_[currentChatId_][row]->getType() != Chat::DataObject::Type::MessageData) {
      return statusImageConnecting_;
   }


   std::shared_ptr<Chat::MessageData> message =
          std::dynamic_pointer_cast<Chat::MessageData>(messages_[currentChatId_][row]);
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
   else if (!link.toString().startsWith(QLatin1Literal("user:"))) {
      QDesktopServices::openUrl(link);
   }
}

void ChatMessagesTextEdit::insertMessage(std::shared_ptr<Chat::DataObject> msg)
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
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [messageId](std::shared_ptr<Chat::DataObject> data){
         return data->getType() == Chat::MessageData::Type::MessageData
                && std::dynamic_pointer_cast<Chat::MessageData>(data)->id() == messageId;
      });

      if (it != messages_[chatId].end()) {
         found = std::dynamic_pointer_cast<Chat::MessageData>(*it);
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
      auto it = std::find_if(messages_[chatId].begin(), messages_[chatId].end(), [id](std::shared_ptr<Chat::DataObject> data){
         return data->getType() == Chat::MessageData::Type::MessageData
                && std::dynamic_pointer_cast<Chat::MessageData>(data)->id() == id;
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

void ChatMessagesTextEdit::onMessagesUpdate(const std::vector<std::shared_ptr<Chat::DataObject>>& messages, bool isFirstFetch)
{
//   for (const auto& message: messages) {
//      messages_[currentChatId_].push_back(message);
//   }
   for (const auto& message : messages) {
      insertMessage(message);
   }
   if (isChatTab_ && QApplication::activeWindow()) {
      for (const auto& data : messages) {
         if (data->getType() == Chat::DataObject::Type::MessageData){
            auto message = std::dynamic_pointer_cast<Chat::MessageData>(data);
            if (messageReadHandler_
                && !(message->state() & (int)Chat::MessageData::State::Read) ){
               messageReadHandler_->onMessageRead(message);
            }
         }
      }
   }
}

void ChatMessagesTextEdit::onRoomMessagesUpdate(const std::vector<std::shared_ptr<Chat::DataObject>>& messages, bool isFirstFetch)
{
   for (const auto& message : messages) {
      insertMessage(message);
   }
   if (isChatTab_ && QApplication::activeWindow()) {
      for (const auto& data : messages) {
         if (data->getType() == Chat::DataObject::Type::MessageData){
            auto message = std::dynamic_pointer_cast<Chat::MessageData>(data);
            if (messageReadHandler_
                && !(message->state() & (int)Chat::MessageData::State::Read) ){
               messageReadHandler_->onMessageRead(message);
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
   if (!element || !element->getDataObject()){
      return;
   }

   std::vector<std::shared_ptr<Chat::DataObject>> displayData;
   bool messageOnly = true;
   for (auto msg_item : element->getChildren()){
      auto item = dynamic_cast<DisplayableDataNode*>(msg_item);
      if (item){
         if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::MessageDataNode){
            auto msg = std::dynamic_pointer_cast<Chat::MessageData>(item->getDataObject());
            if (msg) {
               displayData.push_back(msg);
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
   switch (data->getType()) {
      case Chat::DataObject::Type::RoomData:{
         auto room = std::dynamic_pointer_cast<Chat::RoomData>(data);
         if (room) {
            switchToChat(room->getId(), true);
            onRoomMessagesUpdate(displayData, true);
         }
      }
         break;
      case Chat::DataObject::Type::ContactRecordData: {
         auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
         if (contact) {
            switchToChat(contact->getContactId(), false);
            onMessagesUpdate(displayData, true);
         }
      }
         break;
      case Chat::DataObject::Type::OTCResponseData: {
         auto otcResponse = std::dynamic_pointer_cast<Chat::OTCResponseData>(data);
         if (otcResponse) {
            switchToChat(QString::fromStdString(otcResponse->serverResponseId()), false);
            onMessagesUpdate(displayData, true);
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

   if (!data){
      return;
   }

   switch (data->getType()) {
      case Chat::DataObject::Type::RoomData:{
         auto room = std::dynamic_pointer_cast<Chat::RoomData>(data);
         if (room && room->getId() == currentChatId_){
            messages_.clear();
            clear();
            std::vector<std::shared_ptr<Chat::DataObject>> displayData;
            for (auto msg_item : element->getChildren()){
               auto item = dynamic_cast<DisplayableDataNode*>(msg_item);
               auto msg = item->getDataObject();
               displayData.push_back(msg);
            }
            onRoomMessagesUpdate(displayData, true);
         }
      }
      break;
      case Chat::DataObject::Type::ContactRecordData: {
         auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
         if (contact && contact->getContactId() == currentChatId_){
            messages_.clear();
            clear();
            std::vector<std::shared_ptr<Chat::DataObject>> displayData;
            for (auto msg_item : element->getChildren()){
               auto item = dynamic_cast<DisplayableDataNode*>(msg_item);
               auto msg = item->getDataObject();
               displayData.push_back(msg);
            }
            onMessagesUpdate(displayData, true);
         }
      }
      break;
      default:
         return;
   }
}
