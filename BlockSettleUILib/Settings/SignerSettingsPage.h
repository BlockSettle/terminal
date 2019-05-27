#ifndef __SIGNER_SETTINGS_PAGE_H__
#define __SIGNER_SETTINGS_PAGE_H__

#include <memory>
#include "ConfigDialog.h"
#include "SignersModel.h"


namespace Ui {
   class SignerSettingsPage;
};

class ApplicationSettings;


class SignerSettingsPage : public SettingsPage
{
   Q_OBJECT
public:
   SignerSettingsPage(QWidget* parent = nullptr);
   ~SignerSettingsPage() override;

   void display() override;
   void reset() override;
   void apply() override;
   void initSettings() override;

private slots:
   void runModeChanged(int index);
   void onAsSpendLimitChanged(double);
   void onManageSignerKeys();

signals:
   void signersChanged();

private:
   void onModeChanged(SignContainer::OpMode mode);
   void showHost(bool);
   void showPort(bool);
   void showZmqPubKey(bool);
   void showLimits(bool);
   void showSignerKeySettings(bool);

private:
   std::unique_ptr<Ui::SignerSettingsPage> ui_;
   SignersModel *signersModel_;
};

#endif // __SIGNER_SETTINGS_PAGE_H__
