#ifndef __GENERAL_SETTINGS_PAGE_H__
#define __GENERAL_SETTINGS_PAGE_H__

#include <QWidget>

#include <memory>

namespace Ui {
   class GeneralSettingsPage;
};

class ApplicationSettings;
class WalletsManager;

class GeneralSettingsPage : public QWidget
{
   Q_OBJECT

signals:
   void illformedSettings(bool illformed);

public:
   GeneralSettingsPage(QWidget* parent = nullptr);

   void displaySettings(const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<WalletsManager>& walletsMgr
      , bool displayDefault = false);

   void applyChanges(const std::shared_ptr<ApplicationSettings>& appSettings
      , const std::shared_ptr<WalletsManager>& walletsMgr);

private slots:
   void onSelectLogFile();
   void onSelectMsgLogFile();
   void onLogFileNameEdited(const QString &txt);
   void onGroupBoxClicked(bool);
   void onLogLevelChanged(int);

private:
   void checkSettings();

private:
   Ui::GeneralSettingsPage *ui_;
};

#endif // __GENERAL_SETTINGS_PAGE_H__
