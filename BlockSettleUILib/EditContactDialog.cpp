#include "EditContactDialog.h"
#include "ui_EditContactDialog.h"

const QString kDateTimeStringFormat = QStringLiteral("yyyy-MM-dd");

EditContactDialog::EditContactDialog(const QString &contactId
                                     , const QString &displayName
                                     , const QDateTime &joinDate
                                     , const QString &idKey
                                     , QWidget *parent) :
   QDialog(parent)
 , ui_(new Ui::EditContactDialog())
 , contactId_(contactId)
 , displayName_(displayName)
 , joinDate_(joinDate)
 , idKey_(idKey)
{
   ui_->setupUi(this);

   refillFields();
   connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &EditContactDialog::accept);

   if (displayName_.isEmpty()) {
      ui_->nameOptionalLineEdit->setFocus();
   } else {
      ui_->buttonBox->setFocus();
   }
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

QDateTime EditContactDialog::joinDate() const
{
   return joinDate_;
}

QString EditContactDialog::idKey() const
{
   return idKey_;
}

void EditContactDialog::accept()
{
   displayName_ = ui_->nameOptionalLineEdit->text();
   contactId_ = ui_->userIDLineEdit->text();
   if (!ui_->contactDateLineEdit->text().isEmpty()) {
      joinDate_ = QDateTime::fromString(ui_->contactDateLineEdit->text(), kDateTimeStringFormat);
   }
   idKey_ = ui_->iDKeyLineEdit->text();
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
   move(parentWindowCenter - dialogCenter);
}

void EditContactDialog::refillFields()
{
   ui_->nameOptionalLineEdit->setText(displayName_);
   ui_->userIDLineEdit->setText(contactId_);
   if (joinDate_.isValid()) {
      ui_->contactDateLineEdit->setText(joinDate_.toString(kDateTimeStringFormat));
   }
   ui_->iDKeyLineEdit->setText(idKey_);
}
