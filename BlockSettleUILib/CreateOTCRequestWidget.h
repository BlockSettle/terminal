#ifndef __CREATE_OTC_REQUEST_WIDGET_H__
#define __CREATE_OTC_REQUEST_WIDGET_H__

#include <QWidget>
#include <memory>

#include "ChatCommonTypes.h"

namespace Ui {
   class CreateOTCRequestWidget;
};


class CreateOTCRequestWidget : public QWidget
{
Q_OBJECT

public:
   CreateOTCRequestWidget(QWidget* parent = nullptr);
   ~CreateOTCRequestWidget() override;

   bs::network::ChatOTCSide::Type GetSide() const;
   bs::network::OTCRangeID::Type GetRange() const;
   bool SendAsOwn() const;
   bool ReplyRequired() const;

   void setSubmitButtonEnabled(bool enabled);

private slots:

   void OnSellClicked();
   void OnBuyClicked();

   void OnRangeSelected(int index);
   void onSendAsOwnChanged();

signals:
   void RequestCreated();

private:
   void RequestUpdated();

private:
   std::unique_ptr<Ui::CreateOTCRequestWidget> ui_;
};

#endif // __CREATE_OTC_REQUEST_WIDGET_H__
