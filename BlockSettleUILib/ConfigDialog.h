#ifndef __CONFIG_DIALOG_H__
#define __CONFIG_DIALOG_H__

#include <memory>
#include <QDialog>
#include "ApplicationSettings.h"
#include "ArmoryServersProvider.h"

namespace Ui {
   class ConfigDialog;
}

class SettingsPage : public QWidget
{
   Q_OBJECT

public:
   SettingsPage(QWidget *parent) : QWidget(parent) {}

   virtual void init(const std::shared_ptr<ApplicationSettings> &appSettings
                     , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider) {
      appSettings_ = appSettings;
      armoryServersProvider_ = armoryServersProvider;
      initSettings();
      display();
   }
   virtual void initSettings() {}

   virtual void display() = 0;
   virtual void reset() = 0;
   virtual void apply() = 0;

signals:
   void illformedSettings(bool illformed);

protected:
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
};


class ConfigDialog : public QDialog
{
Q_OBJECT

public:
   ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
     , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
     , QWidget* parent = nullptr);
   ~ConfigDialog() override;

protected:
   void reject() override;

private slots:
   void onDisplayDefault();
   void onAcceptSettings();
   void onSelectionChanged(int currentRow);
   void illformedSettings(bool illformed);

signals:
   void reconnectArmory();

private:
   std::unique_ptr<Ui::ConfigDialog> ui_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   std::vector<SettingsPage *>            pages_;
   ApplicationSettings::State             prevState_;
};

#endif
