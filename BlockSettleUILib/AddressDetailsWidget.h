#ifndef __ADDRESSDETAILSWIDGET_H__
#define __ADDRESSDETAILSWIDGET_H__

#include "Address.h"
#include "ArmoryConnection.h"

#include <QWidget>
#include <QItemSelection>

namespace Ui {
   class AddressDetailsWidget;
}

class QTreeWidgetItem;

class AddressDetailsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit AddressDetailsWidget(QWidget *parent = nullptr);
   ~AddressDetailsWidget();

   void init(const std::shared_ptr<ArmoryConnection> &armory,
             const std::shared_ptr<spdlog::logger> &inLogger);
   void setAddrVal(const bs::Address& inAddrVal);
   void setAddrVal(const QString inAddrVal);
   void loadTransactions();

   enum AddressTreeColumns {
      colDate = 0,
      colTxId = 1,
      colConfs = 2,
      colInputsNum,
      colOutputsNum,
      colOutput,
      colFees,
      colFeePerByte,
      colSize
   };

signals:
   void transactionClicked(QString txId);

protected slots:
   void onTxClicked(QTreeWidgetItem *item, int column);

private:
   void setConfirmationColor(QTreeWidgetItem *item);
   void setOutputColor(QTreeWidgetItem *item);

   Ui::AddressDetailsWidget *ui_;
   bs::Address addrVal;

   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // ADDRESSDETAILSWIDGET_H
