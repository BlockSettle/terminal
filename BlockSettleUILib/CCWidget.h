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
   ~CCWidget() override = default;

   void SetPortfolioModel(const std::shared_ptr<CCPortfolioModel>& model);

private slots:
   void updateTotalAssets();
   void onRowsInserted(const QModelIndex &parent, int first, int last);

private:
   std::shared_ptr<AssetManager> assetManager_;
   Ui::CCWidget* ui;
};

#endif // __CC_WIDGET_H__
