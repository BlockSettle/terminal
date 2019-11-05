#include "SupportDialog.h"

#include <QDesktopServices>

SupportDialog::SupportDialog(QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::SupportDialog)

{
   ui_->setupUi(this);
}

SupportDialog::~SupportDialog() = default;

