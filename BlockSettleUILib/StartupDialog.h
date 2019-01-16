#ifndef __STARTUPDIALOG_H__
#define __STARTUPDIALOG_H__

#include <QDialog>
#include <QLatin1String>

#include <memory>

#include "BtcDefinitions.h"

namespace Ui {
class StartupDialog;
}

class StartupDialog : public QDialog
{
  Q_OBJECT

public:
  enum Pages {
    LicenseAgreement,
    Settings,
  };
  enum class RunMode {
     BlocksettleSN,
     Local,
     Custom
  };

  explicit StartupDialog(bool showLicense, QWidget *parent = nullptr);
  ~StartupDialog() override;

  RunMode runMode() const;
  QString armoryDbIp() const;
  int armoryDbPort() const;
  NetworkType networkType() const;

private slots:
  void onBack();
  void onNext();
  void updateStatus();
  void updatePort();

private:
  std::unique_ptr<Ui::StartupDialog> ui_;
  bool showLicense_;
};

#endif // __STARTUPDIALOG_H__
