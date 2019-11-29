/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ui_CoinControlDialog.h"
#include "CoinControlDialog.h"
#include <QPushButton>
#include "SelectedTransactionInputs.h"


CoinControlDialog::CoinControlDialog(const std::shared_ptr<SelectedTransactionInputs> &inputs, bool allowAutoSel, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::CoinControlDialog())
   , selectedInputs_(inputs)
{
   ui_->setupUi(this);

   connect(ui_->buttonBox, &QDialogButtonBox::accepted, this, &CoinControlDialog::onAccepted);
   connect(ui_->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
   connect(ui_->widgetCoinControl, &CoinControlWidget::coinSelectionChanged, this, &CoinControlDialog::onSelectionChanged);
   ui_->widgetCoinControl->initWidget(inputs, allowAutoSel);
}

CoinControlDialog::~CoinControlDialog() = default;

void CoinControlDialog::onAccepted()
{
   ui_->widgetCoinControl->applyChanges(selectedInputs_);
   accept();
}

void CoinControlDialog::onSelectionChanged(size_t nbSelected, bool autoSelection)
{
   ui_->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(nbSelected > 0 || autoSelection);
}

std::vector<UTXO> CoinControlDialog::selectedInputs() const
{
   if (selectedInputs_->UseAutoSel()) {
      return {};
   }
   return selectedInputs_->GetSelectedTransactions();
}
