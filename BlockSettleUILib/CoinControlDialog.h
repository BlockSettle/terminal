/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __COIN_CONTROL_DIALOG_H__
#define __COIN_CONTROL_DIALOG_H__

#include <memory>
#include <vector>
#include <QDialog>
#include "TxClasses.h"

namespace Ui {
    class CoinControlDialog;
};
class CoinControlWidget;
class SelectedTransactionInputs;

class CoinControlDialog : public QDialog
{
Q_OBJECT

public:
   CoinControlDialog(const std::shared_ptr<SelectedTransactionInputs>& inputs, bool allowAutoSel = true, QWidget* parent = nullptr);
   ~CoinControlDialog() override;

   std::vector<UTXO> selectedInputs() const;

private slots:
   void onAccepted();
   void onSelectionChanged(size_t nbSelected, bool autoSelection);

private:
   std::unique_ptr<Ui::CoinControlDialog> ui_;
   std::shared_ptr<SelectedTransactionInputs> selectedInputs_;
};

#endif // __COIN_CONTROL_DIALOG_H__
