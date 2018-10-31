#ifndef __EXPLORERWIDGET_H__
#define __EXPLORERWIDGET_H__

#include <QWidget>
#include <memory>
#include "TabWithShortcut.h"
#include "ArmoryConnection.h"

namespace Ui {
   class ExplorerWidget;
}

class ExplorerWidget : public TabWithShortcut
{
Q_OBJECT

public:
    ExplorerWidget(QWidget *parent = nullptr);
    ~ExplorerWidget() override;

   void init(const std::shared_ptr<ArmoryConnection> &);
   void shortcutActivated(ShortcutType s) override;

   enum Page {
      BlockPage = 0,
      TxPage = 1,
      AddressPage = 2
   };

protected slots:
   void onSearchStarted();
   void populateTransactionWidget(const BinaryData inHex);
   void onTransactionClicked(QString txId);
   void onAddressClicked(QString addressId);

private:
   std::unique_ptr<Ui::ExplorerWidget> ui_;
   std::shared_ptr<ArmoryConnection>   armory_;
};

#endif // EXPLORERWIDGET_H
