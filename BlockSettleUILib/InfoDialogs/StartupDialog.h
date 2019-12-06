/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __STARTUPDIALOG_H__
#define __STARTUPDIALOG_H__

#include <QDialog>
#include <QLatin1String>

#include <memory>

#include "BtcDefinitions.h"
#include "ApplicationSettings.h"
#include "ArmoryServersProvider.h"
#include "Settings/ArmoryServersWidget.h"

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

  explicit StartupDialog(bool showLicense, QWidget *parent = nullptr);
  ~StartupDialog() override;

  void init(const std::shared_ptr<ApplicationSettings> &appSettings
          , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider);

private slots:
  void onBack();
  void onNext();
  void updateStatus();

  void adjustPosition();

private:
  std::unique_ptr<Ui::StartupDialog> ui_;
  bool showLicense_;

  std::shared_ptr<ApplicationSettings>   appSettings_;
  std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
  ArmoryServersWidget *armoryServersWidget_ {};
};

#endif // __STARTUPDIALOG_H__
