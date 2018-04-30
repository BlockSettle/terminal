#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "ChangeLogDialog.h"
#include "NotificationCenter.h"
#include "TerminalVersion.h"

std::string AboutDialog::terminalVersion_ = TERMINAL_VERSION_STRING;

AboutDialog::AboutDialog(QString changeLogBaseUrl, QWidget* parent)
   : QDialog(parent)
   , ui_(new Ui::AboutDialog)
   , verChecker_(changeLogBaseUrl)
{
   ui_->setupUi(this);

   ui_->label_Version->setText(tr("Version %1 (%2)").arg(QString::fromStdString(terminalVersion_)
      , QString::fromStdString(TERMINAL_BUILD_REVISION)));
   reset();

   connect(ui_->pushButtonCheckUpd, &QPushButton::clicked, this, &AboutDialog::onCheckForUpdates);
   connect(&verChecker_, &bs::VersionChecker::latestVersionLoaded, this, &AboutDialog::latestVerReceived);
   connect(ui_->pushButtonChangeLog, &QPushButton::clicked, this, &AboutDialog::viewChangleLog);
}

void AboutDialog::reset()
{
   ui_->pushButtonChangeLog->hide();
   ui_->pushButtonCheckUpd->setText(tr("Check for updates"));
   ui_->pushButtonCheckUpd->setEnabled(true);
}

void AboutDialog::setTab(int tab)
{
   reset();
   ui_->tabWidget->setCurrentIndex(tab);
}

void AboutDialog::onCheckForUpdates()
{
   ui_->pushButtonCheckUpd->setEnabled(false);
   verChecker_.loadLatestVersion();
}

void AboutDialog::latestVerReceived(bool weAreUpToDate)
{
   if (weAreUpToDate) {
      ui_->pushButtonCheckUpd->setText(tr("You have the latest version"));
   } else {
      QString latestVersion = verChecker_.getLatestVersion();
      ui_->pushButtonCheckUpd->setText(tr("New version %1 is available").arg(latestVersion));
      ui_->pushButtonChangeLog->show();
      NotificationCenter::notify(bs::ui::NotifyType::NewVersion, {latestVersion});
   }
}

void AboutDialog::changeLogReceived(const QString &reqVer, const QStringList &changeLog)
{
   ui_->pushButtonChangeLog->setEnabled(true);
}

void AboutDialog::viewChangleLog()
{
   ChangeLogDialog dlg(verChecker_, this);
   dlg.exec();
}
