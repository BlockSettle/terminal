#ifndef __CONFIG_DIALOG_H__
#define __CONFIG_DIALOG_H__

#include <memory>
#include <QDialog>
#include "ApplicationSettings.h"


namespace Ui {
   class ConfigDialog;
}

class SettingsPage : public QWidget
{
   Q_OBJECT

public:
   SettingsPage(QWidget *parent) : QWidget(parent) {}

   virtual void init(const std::shared_ptr<ApplicationSettings> &appSettings) {
      appSettings_ = appSettings;
      display();
   }
   virtual void display() = 0;
   virtual void reset() = 0;
   virtual void apply() = 0;

signals:
   void illformedSettings(bool illformed);

protected:
   std::shared_ptr<ApplicationSettings>   appSettings_;
};


class ConfigDialog : public QDialog
{
Q_OBJECT

public:
   ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
      , QWidget* parent = nullptr);
   ~ConfigDialog() override;

protected:
   void reject() override;

private slots:
   void onDisplayDefault();
   void onAcceptSettings();
   void onSelectionChanged(int currentRow);
   void illformedSettings(bool illformed);

private:
   std::unique_ptr<Ui::ConfigDialog> ui_;
   std::shared_ptr<ApplicationSettings>   applicationSettings_;
   std::vector<SettingsPage *>            pages_;
   ApplicationSettings::State             prevState_;
};

#endif
