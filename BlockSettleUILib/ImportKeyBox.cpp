/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ImportKeyBox.h"
#include "ui_ImportKeyBox.h"
#include <QShowEvent>
#include <QStyle>
#include <QTimer>

ImportKeyBox::ImportKeyBox(BSMessageBox::Type mbType, const QString& title
   , QWidget* parent)
      : QDialog(parent)
      , ui_(new Ui::ImportKeyBox) {

   ui_->setupUi(this);
   setWindowTitle(QStringLiteral("New ID Key"));
   ui_->labelTitle->setText(title);

   setType(mbType);

   ui_->labelDescription->hide();
   ui_->verticalSpacerDescription->changeSize(100, 0);

   resize(width(), 0);

   connect(ui_->pushButtonOk, &QPushButton::clicked, this, &ImportKeyBox::accept);
   connect(ui_->pushButtonCancel, &QPushButton::clicked, this, &ImportKeyBox::reject);

   setWindowFlags(Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint);
   //layout()->setSizeConstraint(QLayout::SetFixedSize);

   for (QObject *w : ui_->widgetValues->children()) {
      static_cast<QWidget *>(w)->setProperty("h6", true);
   }
}

ImportKeyBox::~ImportKeyBox() = default;

void ImportKeyBox::showEvent( QShowEvent* )
{
   if (parentWidget()) {
      QRect parentRect(parentWidget()->mapToGlobal(QPoint(0, 0)), parentWidget()->size());
      move(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(), parentRect).topLeft());
   }
}

void ImportKeyBox::setOkVisible(bool visible)
{
   ui_->pushButtonOk->setVisible(visible);
}

void ImportKeyBox::setCancelVisible(bool visible)
{
   ui_->pushButtonCancel->setVisible(visible);
}

void ImportKeyBox::setDescription(const QString &desc)
{
   ui_->labelDescription->setText(desc);
   ui_->labelDescription->show();

   ui_->verticalSpacerDescription->changeSize(100, 10);
}

void ImportKeyBox::setAddrPort(const std::string &srvAddrPort)
{
   QStringList addrPort = QString::fromStdString(srvAddrPort).split(QStringLiteral(":"), QString::SkipEmptyParts);
   QString address;
   if (addrPort.size() > 0) {
      address = addrPort.at(0);
   }

   QString port;
   if (addrPort.size() > 1) {
      port = addrPort.at(1);
   }

   setAddress(address);
   setPort(port);
}

void ImportKeyBox::setType(BSMessageBox::Type type) {
   ui_->labelTitle->setProperty("h1", true);
   ui_->pushButtonCancel->hide();
   switch (type) {
   case BSMessageBox::info:
      ui_->labelTitle->setProperty("statusInformationLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_info.png")));
      break;
   case BSMessageBox::success:
      ui_->labelTitle->setProperty("statusSuccessLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_success.png")));
      break;
   case BSMessageBox::question:
      ui_->labelTitle->setProperty("statusWarningLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_question.png")));
      ui_->pushButtonCancel->show();
      break;
   case BSMessageBox::warning:
      ui_->labelTitle->setProperty("statusCautionLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_warning.png")));
      break;
   case BSMessageBox::critical:
      ui_->labelTitle->setProperty("statusImportantLabel", true);
      ui_->labelIcon->setPixmap(QPixmap(QString::fromUtf8("://resources/notification_critical.png")));
      break;
   default:
      break;
   }
}

void ImportKeyBox::setAddress(const QString &address)
{
   ui_->labelAddress->setVisible(!address.isEmpty());
   ui_->labelAddressValue->setVisible(!address.isEmpty());
   ui_->labelAddressValue->setText(address);
}

void ImportKeyBox::setPort(const QString &port)
{
   ui_->labelPort->setVisible(!port.isEmpty());
   ui_->labelPortValue->setVisible(!port.isEmpty());
   ui_->labelPortValue->setText(port);
}

void ImportKeyBox::setNewKey(const QString &newKey)
{
   ui_->labelNewKeyValue->setText(newKey);
}

void ImportKeyBox::setOldKey(const QString &oldKey)
{
//   ui_->labelOldKey->setVisible(!oldKey.isEmpty());
//   ui_->labelOldKeyValue->setVisible(!oldKey.isEmpty());
   ui_->labelOldKeyValue->setText(oldKey.isEmpty() ? tr("<none>") : oldKey);
}

void ImportKeyBox::setConfirmButtonText(const QString &text) {
   ui_->pushButtonOk->setText(text);
}

void ImportKeyBox::setCancelButtonText(const QString &text) {
   ui_->pushButtonCancel->setText(text);
}


