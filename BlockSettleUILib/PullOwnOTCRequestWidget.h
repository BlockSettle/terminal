#ifndef __PULL_OWN_OTC_REQUEST_WIDGET_H__
#define __PULL_OWN_OTC_REQUEST_WIDGET_H__

#include <QWidget>

#include "ChatProtocol/DataObjects/OTCRequestData.h"
#include "CommonTypes.h"
namespace Ui {
    class PullOwnOTCRequestWidget;
};

class PullOwnOTCRequestWidget : public QWidget
{
Q_OBJECT

public:
   PullOwnOTCRequestWidget(QWidget* parent = nullptr );
   ~PullOwnOTCRequestWidget() noexcept override;

   void DisplayActiveOTC(const std::shared_ptr<Chat::OTCRequestData>& otc);
   void DisplaySubmittedOTC(const bs::network::OTCRequest& otc);

private slots:
   void OnPullPressed();

signals:
   void PullOTCRequested(const QString& otcId);

private:
   std::unique_ptr<Ui::PullOwnOTCRequestWidget> ui_;

   QString currentOtcId_;
};

#endif // __PULL_OWN_OTC_REQUEST_WIDGET_H__
