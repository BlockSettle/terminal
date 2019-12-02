/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ABOUT_DIALOG_H__
#define __ABOUT_DIALOG_H__

#include <QDialog>
#include <memory>

#include "VersionChecker.h"

namespace Ui {
    class AboutDialog;
}

class AboutDialog : public QDialog
{
Q_OBJECT

public:
   AboutDialog(QString changeLogBaseUrl, QWidget* parent = nullptr);
   ~AboutDialog() override;

   void setTab(int tab);
   static std::string version() { return terminalVersion_; }

private slots:
   void onCheckForUpdates();
   void latestVerReceived(bool weAreUpToDate);
   void changeLogReceived(const QString &reqVer, const QStringList &changeLog);
   void viewChangleLog();

private:
   void reset();

private:
   std::unique_ptr<Ui::AboutDialog> ui_;
   static std::string   terminalVersion_;
   bs::VersionChecker   verChecker_;
};

#endif // __ABOUT_DIALOG_H__
