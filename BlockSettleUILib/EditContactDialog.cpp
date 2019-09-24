#include "EditContactDialog.h"
#include "ui_EditContactDialog.h"

EditContactDialog::EditContactDialog(const QString &contactId
                                     , const QString &displayName
                                     , const QDateTime &timestamp
                                     , const QString &idKey
                                     , QWidget *parent) :
   QDialog(parent)
 , ui_(new Ui::EditContactDialog())
 , contactId_(contactId)
 , displayName_(displayName)
 , timestamp_(timestamp)
 , idKey_(idKey)
{
   ui_->setupUi(this);

   refillFields();
   connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &EditContactDialog::accept);

   ui_->nameOptionalLineEdit->setFocus();
   ui_->nameOptionalLineEdit->selectAll();
}

EditContactDialog::~EditContactDialog() noexcept = default;

QString EditContactDialog::contactId() const
{
   return contactId_;
}

QString EditContactDialog::displayName() const
{
   return displayName_;
}

QDateTime EditContactDialog::timestamp() const
{
   return timestamp_;
}

QString EditContactDialog::idKey() const
{
   return idKey_;
}

void EditContactDialog::accept()
{
   displayName_ = ui_->nameOptionalLineEdit->text();
   QDialog::accept();
}

void EditContactDialog::reject()
{
   refillFields();
   QDialog::reject();
}

void EditContactDialog::showEvent(QShowEvent *event)
{
   Q_UNUSED(event)
   auto dialogCenter = window()->mapToGlobal(window()->rect().center());
   auto parentWindow = parentWidget()->window();
   auto parentWindowCenter = parentWindow->mapToGlobal(parentWindow->rect().center());
   if (parentWindowCenter == dialogCenter) {
      move(parentWindowCenter - window()->rect().center());
   } else {
      move(parentWindowCenter - dialogCenter);
   }
}

void EditContactDialog::refillFields()
{
   ui_->nameOptionalLineEdit->setText(displayName_);
   ui_->userIDLineEdit->setText(contactId_);
   if (timestamp_.isValid()) {
      ui_->contactDateLineEdit->setText(timestamp_.toString(Qt::SystemLocaleShortDate));
   }
   ui_->iDKeyLineEdit->setText(idKey_);
}
