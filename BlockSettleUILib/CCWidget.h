/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CC_WIDGET_H__
#define __CC_WIDGET_H__

#include <QWidget>
#include <memory>
#include "SignerDefs.h"

namespace Ui {
    class CCWidget;
};

class CCPortfolioModel;
class AssetManager;

class CCWidget : public QWidget
{
Q_OBJECT

public:
   CCWidget(QWidget* parent = nullptr );
   ~CCWidget() override;

   void SetPortfolioModel(const std::shared_ptr<CCPortfolioModel>& model);

   void onWalletBalance(const bs::sync::WalletBalanceData&);
   void onPriceChanged(const std::string& currency, double price);
   void onBasePriceChanged(const std::string &currency, double price);
   void onBalance(const std::string& currency, double balance);

private slots:
   void updateTotalAssets();
   void onRowsInserted(const QModelIndex &parent, int first, int last);

private:
   void updateTotalBalances();

private:
   std::unique_ptr<Ui::CCWidget> ui_;
   std::map<std::string, double> walletBalance_;
   std::map<std::string, double> fxBalance_;
   std::map<std::string, double> fxPrices_;
   std::string baseCur_;
   double      basePrice_{ 0 };
};

#endif // __CC_WIDGET_H__
