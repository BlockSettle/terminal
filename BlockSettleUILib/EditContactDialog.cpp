#include "EditContactDialog.h"
#include "ui_EditContactDialog.h"

EditContactDialog::EditContactDialog(QWidget *parent) :
   QDialog(parent),
   ui_(new Ui::EditContactDialog())
{
   ui_->setupUi(this);
}

EditContactDialog::~EditContactDialog() noexcept = default;
