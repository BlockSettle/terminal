#ifndef __SIGNER_SETTINGS_PAGE_H__
#define __SIGNER_SETTINGS_PAGE_H__

#include <memory>
#include <QWidget>


namespace Ui {
   class SignerSettingsPage;
};

class ApplicationSettings;


class SignerSettingsPage : public QWidget
{
Q_OBJECT

public:
   SignerSettingsPage(QWidget* parent = nullptr);
   ~SignerSettingsPage() override;

   void setAppSettings(const std::shared_ptr<ApplicationSettings> &);
   void displaySettings(bool displayDefault = false);
   void applyChanges();

private slots:
   void runModeChanged(int index);
   void onOfflineDirSel();
   void onZmqPubKeySel();
   void onAsSpendLimitChanged(double);

private:
   void onModeChanged(int index, bool displayDefault);
   void saveZmqRemotePubKey();
   void showHost(bool);
   void showPort(bool);
   void showZmqPubKey(bool);
   void showOfflineDir(bool);
   void showLimits(bool);

private:
   std::unique_ptr<Ui::SignerSettingsPage> ui_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
};

#endif // __SIGNER_SETTINGS_PAGE_H__
