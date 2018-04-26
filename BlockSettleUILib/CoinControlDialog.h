#ifndef __COIN_CONTROL_DIALOG_H__
#define __COIN_CONTROL_DIALOG_H__

#include <QDialog>
#include "WalletsManager.h"
#include <memory>

namespace Ui {
    class CoinControlDialog;
};
class CoinControlWidget;
class SelectedTransactionInputs;

class CoinControlDialog : public QDialog
{
Q_OBJECT

public:
   CoinControlDialog(const std::shared_ptr<SelectedTransactionInputs>& inputs, QWidget* parent = nullptr);
   ~CoinControlDialog() override = default;

private slots:
   void onAccepted();
   void onSelectionChanged(size_t nbSelected);

private:
   Ui::CoinControlDialog*  ui_;
   std::shared_ptr<SelectedTransactionInputs> selectedInputs_;
};

#endif // __COIN_CONTROL_DIALOG_H__
