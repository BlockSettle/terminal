#include "BSMessageBox.h"
#include "ui_BSMessageBox.h"
#include <QDebug>
#include <QShowEvent>

BSMessageBox::BSMessageBox(const QString& title, const QString& windowTitle
   , const QString& text, messageBoxType mbType
   , const QString& details, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::BSMessageBox) {
   ui_->setupUi(this);
   setWindowTitle(windowTitle);
   ui_->labelTitle->setText(title);
   ui_->labelText->setText(text);
   setType(mbType);

   if (details.isEmpty()) {
      ui_->pushButtonShowDetails->hide();
   } else {
      connect(ui_->pushButtonShowDetails, &QPushButton::clicked, this, &BSMessageBox::onDetailsPressed);
      ui_->labelDetails->setText(details);
   }
   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &BSMessageBox::accept);
   setFixedSize(400, 290);
   hideDetails();

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
   qDebug() << "HideDetails" << this->height() << ui_->verticalWidgetDetails->height();

   this->setFixedHeight(this->height() - 100); // hardcoding the details height makes show/hide work more consistently
}

void BSMessageBox::showDetails() {
   ui_->verticalWidgetDetails->show();
   qDebug() << "ShowDetails" << this->height() << ui_->verticalWidgetDetails->height();

   this->setFixedHeight(this->height() + 100); // hardcoding the details height makes show/hide work more consistently
}

void BSMessageBox::setType(messageBoxType type) {
   ui_->labelTitle->setProperty("h1", true);
   ui_->labelText->setProperty("h6", true);
   switch (type) {
   case mbInfo:
      ui_->labelTitle->setProperty("statusInformationLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_info.png")));
      break;
   case mbSuccess:
      ui_->labelTitle->setProperty("statusSuccessLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_success.png")));
      break;
   case mbQuestion:
      ui_->labelTitle->setProperty("statusWarningLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_question.png")));
      break;
   case mbWarning:
      ui_->labelTitle->setProperty("statusCautionLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_warning.png")));
      break;
   case mbCritical:
      ui_->labelTitle->setProperty("statusImportantLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_critical.png")));
      break;
   default:
      break;
   }
}
