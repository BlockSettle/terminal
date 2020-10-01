/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __STARTUPDIALOG_H__
#define __STARTUPDIALOG_H__

#include <memory>
#include <QDialog>

#include "ApplicationSettings.h"
#include "BtcDefinitions.h"

namespace Ui {
   class StartupDialog;
}

class ApplicationSettings;

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

  void init(const std::shared_ptr<ApplicationSettings> &appSettings);
  void applySelectedConnectivity();

private slots:
  void onBack();
  void onNext();
  void onConnectivitySelectionChanged();

private:
  void updateStatus();
  void adjustPosition();
  void setupConnectivityList();
  NetworkType getSelectedNetworkType() const;

private:
  std::unique_ptr<Ui::StartupDialog> ui_;
  bool showLicense_;

  std::shared_ptr<ApplicationSettings>   appSettings_;
};

#endif // __STARTUPDIALOG_H__
