#include "BSMessageBox.h"
#include "ui_BSMessageBox.h"
#include <QDebug>
#include <QShowEvent>

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
   // then recalculate the height, this is needed in case the text
   // in the message box doesn't fit in its height, this call
   // will increase the height of the message box to fit the text
   setFixedHeight(sizeHint().height()+15);
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
