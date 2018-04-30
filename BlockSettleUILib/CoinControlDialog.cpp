#include <QPushButton>
#include "CoinControlDialog.h"
#include "ui_CoinControlDialog.h"

CoinControlDialog::CoinControlDialog(const std::shared_ptr<SelectedTransactionInputs> &inputs, QWidget* parent)
 : QDialog(parent)
 , ui_(new Ui::CoinControlDialog())
 , selectedInputs_(inputs)
{
   ui_->setupUi(this);

   connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &CoinControlDialog::onAccepted);
   connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
   connect(ui_->widgetCoinControl, &CoinControlWidget::coinSelectionChanged, this, &CoinControlDialog::onSelectionChanged);
   ui_->widgetCoinControl->initWidget(inputs);
}

void CoinControlDialog::onAccepted()
{
   ui_->widgetCoinControl->applyChanges(selectedInputs_);
   accept();
}

void CoinControlDialog::onSelectionChanged(size_t nbSelected)
{
   ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(nbSelected > 0);
}
