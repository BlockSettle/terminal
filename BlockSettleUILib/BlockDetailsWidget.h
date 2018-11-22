#ifndef __BLOCKDETAILSWIDGET_H__
#define __BLOCKDETAILSWIDGET_H__

#include <QWidget>
#include <memory>

namespace Ui {
   class BlockDetailsWidget;
}

class BlockDetailsWidget : public QWidget
{
   Q_OBJECT

public:
   explicit BlockDetailsWidget(QWidget *parent = nullptr);
   ~BlockDetailsWidget() override;

private:
   std::unique_ptr<Ui::BlockDetailsWidget> ui_;
};

#endif // __BLOCKDETAILSWIDGET_H__
