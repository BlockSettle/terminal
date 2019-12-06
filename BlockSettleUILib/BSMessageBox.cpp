/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BSMessageBox.h"
#include "ui_BSMessageBox.h"
#include <QShowEvent>
#include <QStyle>
#include <QTimer>

QString BSMessageBox::kUrlColor = QLatin1String("#ffffff");

// Basic constructor, sets message box type, title and text
BSMessageBox::BSMessageBox(Type mbType, const QString& title
   , const QString& text, QWidget* parent)
   : BSMessageBox(mbType, title, text, QString(), QString(), parent) {
}

// This constructor sets message box type, title, text and description.
BSMessageBox::BSMessageBox(Type mbType
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
BSMessageBox::BSMessageBox(Type mbType, const QString& title
   , const QString& text, const QString& description
   , const QString& details, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::BSMessageBox) {
   ui_->setupUi(this);
   setWindowTitle(title);
   ui_->labelTitle->setText(text);
   ui_->labelText->setText(description);
   setType(mbType);
   resize(width(), 0);

   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
      ui_->verticalWidgetDetails->setMaximumHeight(0);
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &BSMessageBox::onDetailsPressed);
      ui_->labelDetails->setText(details);
      ui_->verticalWidgetDetails->setMaximumHeight(SHRT_MAX);
   }
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &BSMessageBox::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &BSMessageBox::reject);

   setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
   //layout()->setSizeConstraint(QLayout::SetFixedSize);

   // hide the details part of the message box
   hideDetails();
}

BSMessageBox::~BSMessageBox() = default;

void BSMessageBox::showEvent( QShowEvent* )
{
   if (parentWidget()) {
      QRect parentRect(parentWidget()->mapToGlobal(QPoint(0, 0)), parentWidget()->size());
      QRect windowGeometry = geometry();
      windowGeometry.moveCenter(parentRect.center());
   }
}

void BSMessageBox::setOkVisible(bool visible)
{
   ui_->pushButtonOk->setVisible(visible);
}

void BSMessageBox::setCancelVisible(bool visible)
{
   ui_->pushButtonCancel->setVisible(visible);
}

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
   adjustSize();
}

void BSMessageBox::showDetails() {
   ui_->verticalWidgetDetails->show();
   adjustSize();
}

void BSMessageBox::setLabelTextFormat(Qt::TextFormat tf) {
   ui_->labelText->setTextFormat(tf);
}

void BSMessageBox::setType(Type type) {
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
   : BSMessageBox(BSMessageBox::critical, tr("Failed to sign"),
      tr("Transaction signing failure"), details
      , parent) {
}

MessageBoxExpTimeout::MessageBoxExpTimeout(QWidget *parent)
   : BSMessageBox(BSMessageBox::warning, tr("Explorer Timeout"),
      tr("Explorer Timeout"), tr("Armory has timed out. Cannot resolve query.")
      , parent) {
}

MessageBoxWalletCreateAbort::MessageBoxWalletCreateAbort(QWidget *parent)
   : BSMessageBox(BSMessageBox::question, tr("Warning"), tr("Abort Wallet Creation?")
      , tr("The Wallet will not be created if you don't complete the procedure.\n\n"
         "Are you sure you want to abort the Wallet Creation process?"), parent) {
   setConfirmButtonText(QObject::tr("Abort"));
   setCancelButtonText(QObject::tr("Back"));
}
