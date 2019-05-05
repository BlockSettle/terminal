#ifndef __SIGNER_SETTINGS_PAGE_H__
#define __SIGNER_SETTINGS_PAGE_H__

#include <memory>
#include "ConfigDialog.h"


namespace Ui {
   class SignerSettingsPage;
};

class ApplicationSettings;


class SignerSettingsPage : public SettingsPage
{
public:
   SignerSettingsPage(QWidget* parent = nullptr);
   ~SignerSettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;

private slots:
   void runModeChanged(int index);
   void onAsSpendLimitChanged(double);
   void onManageSignerKeys();

private:
   void onModeChanged(int index);
   void showHost(bool);
   void showPort(bool);
   void showZmqPubKey(bool);
   void showLimits(bool);
   void showSignerKeySettings(bool);

private:
   std::unique_ptr<Ui::SignerSettingsPage> ui_;
};

#endif // __SIGNER_SETTINGS_PAGE_H__
