#ifndef __TRANSACTIONS_WIDGET_UI_H__
#define __TRANSACTIONS_WIDGET_UI_H__

#include <QWidget>

#include <memory>

#include "TabWithShortcut.h"

class TransactionsProxy;
class TransactionsViewModel;
class TransactionsSortFilterModel;

namespace Ui {
    class TransactionsWidget;
};

class TransactionsWidget : public TabWithShortcut
{
Q_OBJECT

public:
   TransactionsWidget(QWidget* parent = nullptr );
   ~TransactionsWidget() override = default;

   void SetTransactionsModel(const std::shared_ptr<TransactionsViewModel> &);

   void shortcutActivated(ShortcutType s) override;

private slots:
   void showTransactionDetails(const QModelIndex& index);
   void updateResultCount();
   void walletsChanged();
   void walletsFilterChanged(int index);
   void onEnterKeyInTrxPressed(const QModelIndex &index);

private:
   Ui::TransactionsWidget* ui;

private:
   std::shared_ptr<TransactionsViewModel> transactionsModel_;
   TransactionsSortFilterModel            *sortFilterModel_;
};


#endif // __TRANSACTIONS_WIDGET_UI_H__
