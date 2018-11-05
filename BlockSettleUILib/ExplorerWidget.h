#ifndef __EXPLORERWIDGET_H__
#define __EXPLORERWIDGET_H__

#include <memory>
#include <QWidget>
#include "ArmoryConnection.h"

namespace Ui {
class ExplorerWidget;
}

class BlocksViewModel;

class ExplorerWidget : public QWidget
{
   Q_OBJECT

public:
   explicit ExplorerWidget(QWidget *parent = nullptr);
   ~ExplorerWidget();

   void init(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<ArmoryConnection> &armory);

private slots:
   void onArmoryStateChanged(ArmoryConnection::State);
   void onSearchReturnPressed();

private:
   enum class Pages {
      Overview,
      Block,
      Transaction,
      Address,
   };

   void search(const QString& text);
   void showBlockHeader(ClientClasses::BlockHeader& blockHeader);
   void updateOverview();

   std::unique_ptr<Ui::ExplorerWidget> ui_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<ArmoryConnection> armory_;
   BlocksViewModel* blocksViewModel_{};
};

#endif // __EXPLORERWIDGET_H__
