#ifndef __PULL_OWN_OTC_REQUEST_WIDGET_H__
#define __PULL_OWN_OTC_REQUEST_WIDGET_H__

#include <QWidget>

#include "CommonTypes.h"
#include "chat.pb.h"

namespace Ui {
    class PullOwnOTCRequestWidget;
};

class PullOwnOTCRequestWidget : public QWidget
{
Q_OBJECT

public:
   PullOwnOTCRequestWidget(QWidget* parent = nullptr );
   ~PullOwnOTCRequestWidget() noexcept override;

   void setRequestData(const std::shared_ptr<Chat::Data>& otc);

private slots:
   void OnPullPressed();

signals:
   void PullOTCRequested();

private:
   std::unique_ptr<Ui::PullOwnOTCRequestWidget> ui_;
};

#endif // __PULL_OWN_OTC_REQUEST_WIDGET_H__
