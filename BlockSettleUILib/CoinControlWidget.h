#ifndef __COIN_CONTROL_WIDGET_H__
#define __COIN_CONTROL_WIDGET_H__

#include <QWidget>
#include <memory>
#include "WalletsManager.h"

namespace Ui {
	class CoinControlWidget;
};

class CoinControlModel;
class SelectedTransactionInputs;

class CoinControlWidget : public QWidget
{
Q_OBJECT
public:
   CoinControlWidget(QWidget* parent = nullptr );
   ~CoinControlWidget() override = default;

   void initWidget(const std::shared_ptr<SelectedTransactionInputs> &);
   void applyChanges(const std::shared_ptr<SelectedTransactionInputs> &);

signals:
   void coinSelectionChanged(size_t currentlySelected);

private slots:
   void updateSelectedTotals();
   void onAutoSelClicked(int state);

private:
   Ui::CoinControlWidget* ui_;

   CoinControlModel *coinControlModel_;
};


#include <QPainter>
#include <QHeaderView>
#include <QStyleOptionButton>
#include <QMouseEvent>
#ifndef MAXSIZE_T
#  define MAXSIZE_T  ((size_t)-1)
#endif

class CCHeader : public QHeaderView
{
   Q_OBJECT
public:
   CCHeader(size_t totalTxCount, Qt::Orientation orient, QWidget *parent = nullptr)
	  : QHeaderView(orient, parent), totalTxCount_(totalTxCount) {}

protected:
   void paintSection(QPainter *painter, const QRect &rect, int logIndex) const {
	  painter->save();
	  QHeaderView::paintSection(painter, rect, logIndex);
	  painter->restore();
	  if (logIndex == 0) {
		 QStyleOptionButton option;
		 option.rect = QRect(2, 2, height() - 4, height() - 4);

		 switch (state_)
		 {
		 case Qt::Checked:
			option.state = QStyle::State_Enabled | QStyle::State_On;
			break;
		 case Qt::Unchecked:
			option.state = QStyle::State_Enabled | QStyle::State_Off;
			break;
		 case Qt::PartiallyChecked:
		 default:
			option.state = QStyle::State_Enabled | QStyle::State_NoChange;
			break;
		 }
		 style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &option, painter);
	  }
   }

   void mousePressEvent(QMouseEvent *event) {
	  if (QRect(0, 0, height(), height()).contains(event->x(), event->y())) {
		 if (state_ == Qt::Unchecked) {
			state_ = Qt::Checked;
		 }
		 else {
			state_ = Qt::Unchecked;
		 }
		 emit stateChanged(state_);
		 update();
	  }
	  QHeaderView::mousePressEvent(event);
   }

private:
   Qt::CheckState state_ = Qt::Unchecked;
   size_t         totalTxCount_;

signals:
   void stateChanged(int);

public slots:
   void onSelectionChanged(size_t nbSelected) {
	  if (!nbSelected) {
		 state_ = Qt::Unchecked;
	  }
	  else if (nbSelected < totalTxCount_) {
		 state_ = Qt::PartiallyChecked;
	  }
	  else if ((nbSelected == totalTxCount_) || (nbSelected == MAXSIZE_T)) {
		 state_ = Qt::Checked;
	  }
	  update();
   }
};

#endif // __COIN_CONTROL_WIDGET_H__
