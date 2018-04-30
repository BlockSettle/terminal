#ifndef __BASE_DEALER_SETTLEMENT_DIALOG_H__
#define __BASE_DEALER_SETTLEMENT_DIALOG_H__

#include <QDateTime>
#include <QDialog>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QWidget>

#include <chrono>
#include <memory>

namespace spdlog {
   class logger;
}
namespace bs {
   class SettlementContainer;
}

class BaseDealerSettlementDialog : public QDialog
{
Q_OBJECT

public:
   BaseDealerSettlementDialog(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::SettlementContainer> &, QWidget* parent = nullptr);
   ~BaseDealerSettlementDialog() noexcept override = default;

   BaseDealerSettlementDialog(const BaseDealerSettlementDialog&) = delete;
   BaseDealerSettlementDialog& operator = (const BaseDealerSettlementDialog&) = delete;

   BaseDealerSettlementDialog(BaseDealerSettlementDialog&&) = delete;
   BaseDealerSettlementDialog& operator = (BaseDealerSettlementDialog&&) = delete;

protected slots:
   void onTimerTick(int msCurrent, int msDuration);
   void onTimerStarted(int msDuration);
   void onTimerStopped();

protected:
   void connectToProgressBar(QProgressBar *progressBar);
   void connectToHintLabel(QLabel *hintLabel, QLabel *errorLabel);

   void setHintText(const QString& hint);
   void setCriticalHintMessage(const QString& hint);

   bool isMessageCritical() const { return hintSetToCritical_; }

protected:
   std::shared_ptr<spdlog::logger>  logger_;

private:
   std::shared_ptr<bs::SettlementContainer>  settlContainer_;
   QProgressBar   *progressBar_ = nullptr;
   QLabel         *hintLabel_ = nullptr;
   QLabel         *errorLabel_ = nullptr;
   bool           hintSetToCritical_ = false;
};

#endif // __BASE_DEALER_SETTLEMENT_DIALOG_H__
