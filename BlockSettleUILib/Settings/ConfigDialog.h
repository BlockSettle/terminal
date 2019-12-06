/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CONFIG_DIALOG_H__
#define __CONFIG_DIALOG_H__

#include <memory>
#include <QDialog>
#include "ApplicationSettings.h"
#include "ArmoryServersProvider.h"
#include "SignContainer.h"

class ArmoryServersProvider;
class SignersProvider;

namespace Ui {
   class ConfigDialog;
}

class SettingsPage : public QWidget
{
   Q_OBJECT

public:
   SettingsPage(QWidget *parent);

   virtual void init(const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
      , const std::shared_ptr<SignersProvider> &signersProvider
      , std::shared_ptr<SignContainer> signContainer);

public slots:
   virtual void initSettings() {}
   virtual void display() = 0;
   virtual void reset() = 0;
   virtual void apply() = 0;

signals:
   void illformedSettings(bool illformed);

protected:
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   std::shared_ptr<SignersProvider>       signersProvider_;
   std::shared_ptr<SignContainer>         signContainer_;
};


class ConfigDialog : public QDialog
{
Q_OBJECT

public:
   ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
     , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
     , const std::shared_ptr<SignersProvider> &signersProvider
     , std::shared_ptr<SignContainer> signContainer
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
   std::shared_ptr<SignersProvider>       signersProvider_;
   std::vector<SettingsPage *>            pages_;
   ApplicationSettings::State             prevState_;
   std::shared_ptr<SignContainer>         signContainer_;

};

#endif
