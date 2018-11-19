#ifndef __EXPLORERWIDGET_H__
#define __EXPLORERWIDGET_H__

#include "TabWithShortcut.h"
#include "ArmoryConnection.h"

#include <QWidget>
#include <memory>

namespace Ui {
   class ExplorerWidget;
}

class ExplorerWidget : public TabWithShortcut
{
Q_OBJECT

public:
    ExplorerWidget(QWidget *parent = nullptr);
    ~ExplorerWidget() override;

   void init(const std::shared_ptr<ArmoryConnection> &armory,
             const std::shared_ptr<spdlog::logger> &inLogger);
   void shortcutActivated(ShortcutType s) override;

   enum Page {
      BlockPage = 0,
      TxPage,
      AddressPage
   };

protected slots:
   void onSearchStarted();
   void onTransactionClicked(QString txId);
   void onAddressClicked(QString addressId);
   void onReset();

private:
   std::unique_ptr<Ui::ExplorerWidget> ui_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger>     logger_;
};

#endif // EXPLORERWIDGET_H
