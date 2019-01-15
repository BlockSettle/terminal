#include "BSMessageBox.h"
#include "ui_BSMessageBox.h"
#include <QDebug>
#include <QShowEvent>
#include <QTimer>

// Basic constructor, sets message box type, title and text
BSMessageBox::BSMessageBox(messageBoxType mbType, const QString& title
   , const QString& text, QWidget* parent)
   : BSMessageBox(mbType, title, text, QString(), QString(), parent) {
}

// This constructor sets message box type, title, text and description.
BSMessageBox::BSMessageBox(messageBoxType mbType
   , const QString& title, const QString& text
   , const QString& description, QWidget* parent) 
   : BSMessageBox(mbType, title, text, description, QString(), parent) {
}

// Constructor parameters:
// mbType - specifies the message box type: info, success, question, warning, critical
// title - window title of the message box
// text - text with larger font and colored based on mbType
// description - text with smaller font and standard gray color placed below colored text
// details - when specified 'Show Details' buttons shows and when clicked the
// message box expands to show another text area with a scroll bar
BSMessageBox::BSMessageBox(messageBoxType mbType, const QString& title
   , const QString& text, const QString& description 
   , const QString& details, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::BSMessageBox) {
   ui_->setupUi(this);
   setWindowTitle(title);
   ui_->labelTitle->setText(text);
   ui_->labelText->setText(description);
   setType(mbType);

   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &BSMessageBox::onDetailsPressed);
      ui_->labelDetails->setText(details);
   }
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &BSMessageBox::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &BSMessageBox::reject);

   // set fixed width first to make sure the width of the message box will not change
   setFixedWidth(400);
   // hide the details part of the message box
   hideDetails();
   adjustSize();
   // not sure why but this helps size the messagebox 
   // correctly based how much text it contains
   // putting it in a timer makes sure all resizing has been
   // finished by adjustSize()
   QTimer::singleShot(80, [=] {
      showDetails();
      hideDetails();

      // now that we had resized the dialog it
      // might not be perfectly centerred anymore
      // so center it again
      QTimer::singleShot(5, [=] {
         auto p = this->parent();
         // only resize if the message box is larger than 300px in height
         if (p && this->height() > 300) {
            auto w = qobject_cast<QWidget *>(p);
            auto parentRect = w->geometry();
            auto parentCenter = parentRect.center();
            auto myCenter = mapToGlobal(rect().center());
            auto movePoint = parentCenter - myCenter;
            //qDebug() << parentRect << pos() << parentCenter << myCenter << movePoint;
             move(pos() + movePoint);
         }
      });
   });
}

BSMessageBox::~BSMessageBox() = default;

void BSMessageBox::onDetailsPressed() {
   if (detailsVisible()) {
      ui_->pushButtonShowDetails->setText(tr("Show Details"));
      hideDetails();
   } else {
      ui_->pushButtonShowDetails->setText(tr("Hide Details"));
      showDetails();
   }
}

bool BSMessageBox::detailsVisible() const {
   return ui_->verticalWidgetDetails->isVisible();
}

void BSMessageBox::hideDetails() {
   ui_->verticalWidgetDetails->hide();

   this->setFixedHeight(this->height() - 100); // hardcoding the details height makes show/hide work more consistently
}

void BSMessageBox::showDetails() {
   ui_->verticalWidgetDetails->show();

   this->setFixedHeight(this->height() + 100); // hardcoding the details height makes show/hide work more consistently
}

void BSMessageBox::setLabelTextFormat(Qt::TextFormat tf) {
   ui_->labelText->setTextFormat(tf);
}

void BSMessageBox::setType(messageBoxType type) {
   ui_->labelTitle->setProperty("h1", true);
   ui_->labelText->setProperty("h6", true);
   ui_->pushButtonCancel->hide();
   switch (type) {
   case info:
      ui_->labelTitle->setProperty("statusInformationLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_info.png")));
      break;
   case success:
      ui_->labelTitle->setProperty("statusSuccessLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_success.png")));
      break;
   case question:
      ui_->labelTitle->setProperty("statusWarningLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_question.png")));
      ui_->pushButtonCancel->show();
      break;
   case warning:
      ui_->labelTitle->setProperty("statusCautionLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_warning.png")));
      break;
   case critical:
      ui_->labelTitle->setProperty("statusImportantLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_critical.png")));
      break;
   default:
      break;
   }
}

void BSMessageBox::setConfirmButtonText(const QString &text) {
   ui_->pushButtonOk->setText(text); 
}

void BSMessageBox::setCancelButtonText(const QString &text) {
   ui_->pushButtonCancel->setText(text); 
}

MessageBoxCCWalletQuestion::MessageBoxCCWalletQuestion(const QString &ccProduct, QWidget *parent)
   : BSMessageBox(BSMessageBox::question, tr("Private Market Wallet"), tr("Create %1 Wallet").arg(ccProduct)
      , tr("Your wallet does not have a branch in which to hold %1 tokens, would you like to create it?")
      .arg(ccProduct), parent) { 
}

MessageBoxBroadcastError::MessageBoxBroadcastError(const QString &details, QWidget *parent)
   : BSMessageBox(BSMessageBox::critical, tr("Broadcast Failure"),
      tr("Failed to Sign Transaction"), tr("Error occured when signing a transaction.")
      , details, parent) {
}

MessageBoxAuthNotice::MessageBoxAuthNotice(QWidget *parent)
   : BSMessageBox(BSMessageBox::info
                  , tr("Auth eID")
                  , tr("Signing with Auth eID")
                  , tr("Auth eID is a convenient alternative to passwords. "
                       "Instead of entering a password, BlockSettle Terminal issues a secure notification to mobile devices attached to your wallet's Auth eID account. "
                       "You may then sign wallet-related requests via a press of a button in the Auth eID app on your mobile device(s)."
                       "<br><br>You may add or remove devices to your Auth eID accounts as required by the user, and users may have multiple devices on one account. "
                       "Auth eID requires the user to be vigilant with devices using Auth eID. "
                       "If a device is damaged or lost, the user will be unable to sign Auth eID requests, and the wallet will become unusable."
                       "<br><br>Auth eID is not a wallet backup! No wallet data is stored with Auth eID. "
                       "Therefore, you must maintain proper backups of your wallet's Root Private Key (RPK). "
                       "In the event that all mobile devices attached to a wallet are damaged or lost, the RPK may be used to create a duplicate wallet. "
                       "You may then attach a password or your Auth eID account to the wallet."
                       "<br><br>Auth eID, like any software, is susceptible to malware, although keyloggers will serve no purpose. "
                       "Please keep your mobile devices up-to-date with the latest software updates, and never install software offered outside your device's app store."
                       "<br><br>For more information, please consult:<br><a href=\"http://pubb.blocksettle.com/PDF/AutheID%20Getting%20Started.pdf\"><span style=\"color:white;\">Getting Started With Auth eID</span></a>.")
                  , parent) {
   // use rich text because of the hyperlink
   setLabelTextFormat(Qt::RichText);
}

MessageBoxWalletCreateAbort::MessageBoxWalletCreateAbort(QWidget *parent)
   : BSMessageBox(BSMessageBox::question, tr("Warning"), tr("Abort Wallet Creation?")
      , tr("The Wallet will not be created if you don't complete the procedure.\n\n"
         "Are you sure you want to abort the Wallet Creation process?"), parent) {
   setConfirmButtonText(QObject::tr("Abort"));
   setCancelButtonText(QObject::tr("Back"));
}
