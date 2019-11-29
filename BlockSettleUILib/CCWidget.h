/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CC_WIDGET_H__
#define __CC_WIDGET_H__

#include <QWidget>
#include <memory>

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

private slots:
   void updateTotalAssets();
   void onRowsInserted(const QModelIndex &parent, int first, int last);

private:
   std::shared_ptr<AssetManager> assetManager_;
   std::unique_ptr<Ui::CCWidget> ui_;
};

#endif // __CC_WIDGET_H__
