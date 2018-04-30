#include "BuySellCCWidget.h"
#include "ui_BuySellCCWidget.h"

#include "UiUtils.h"

BuySellCCWidget::BuySellCCWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::BuySellCCWidget())
{
   ui_->setupUi(this);
   connect(ui_->comboBoxWallets, &QComboBox::currentTextChanged, [=] {
      ui_->labelBallanceValue->setText(UiUtils::displayAmount(ui_->comboBoxWallets->currentData(UiUtils::WalletBalanceRole).toDouble()));
   });
}

BuySellCCWidget::~BuySellCCWidget()
{}

void BuySellCCWidget::resetView(bool sell)
{
   ui_->lineEditQuantity->clear();

   ui_->labelEstimated->setText(sell ? tr("Estimated Value") : tr("Est. Buying Power"));

   ui_->pushButtonAdvanced->setVisible(!sell);
}
