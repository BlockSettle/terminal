/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __EXPLORERWIDGET_H__
#define __EXPLORERWIDGET_H__

#include "TabWithShortcut.h"
#include "ArmoryConnection.h"
#include "Wallets/SignerDefs.h"

#include <QWidget>
#include <memory>

namespace Ui {
   class ExplorerWidget;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class AuthAddressManager;
class CCFileManager;

class ExplorerWidget : public TabWithShortcut
{
Q_OBJECT

public:
    ExplorerWidget(QWidget *parent = nullptr);
    ~ExplorerWidget() override;

   void init(const std::shared_ptr<spdlog::logger>&);
   void shortcutActivated(ShortcutType s) override;

   enum Page {
      BlockPage = 0,
      TxPage,
      AddressPage
   };

   void mousePressEvent(QMouseEvent *event) override;

   void onNewBlock(unsigned int blockNum);
   void onAddressHistory(const bs::Address&, uint32_t curBlock
      , const std::vector<bs::TXEntry>&);
   void onTXDetails(const std::vector<bs::sync::TXWalletDetails>&);

signals:
   void needAddressHistory(const bs::Address&);
   void needTXDetails(const std::vector<bs::sync::TXWallet>&, bool useCache, const bs::Address&);

protected slots:
   void onSearchStarted(bool saveToHistory);
   void onExpTimeout();
   void onTransactionClicked(QString txId);
   void onAddressClicked(QString addressId);
   void onReset();
   void onBackButtonClicked();
   void onForwardButtonClicked();

private:
   bool canGoBack() const;
   bool canGoForward() const;
   void setTransaction(const QString &txId);
   void pushTransactionHistory(QString itemId);
   void truncateSearchHistory(int position = -1);
   void clearSearchHistory();

private:
   std::unique_ptr<Ui::ExplorerWidget> ui_;
   std::unique_ptr<QTimer>             expTimer_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<AuthAddressManager> authMgr_;
   std::vector<std::string>            searchHistory_;
   int                                 searchHistoryPosition_{-1};
};

#endif // EXPLORERWIDGET_H
