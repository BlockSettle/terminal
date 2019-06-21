#include "EditContactDialog.h"
#include "ui_EditContactDialog.h"

EditContactDialog::EditContactDialog(const QString &userId
      , const QString &displayName
      , const QDateTime &joinDate
      , const QString &info
      , QWidget *parent) :
   QDialog(parent)
 , ui_(new Ui::EditContactDialog())
 , userId_(userId)
 , displayName_(displayName)
 , joinDate_(joinDate)
 , info_(info)
{
   ui_->setupUi(this);
}

EditContactDialog::~EditContactDialog() noexcept = default;

QString EditContactDialog::userId() const
{
   return userId_;
}

QString EditContactDialog::displayName() const
{
   return displayName_;
}

QDateTime EditContactDialog::joinDate() const
{
   return joinDate_;
}

QString EditContactDialog::info() const
{
   return info_;
}
