/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
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
      , const std::shared_ptr<SignContainer> &signContainer
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr);

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
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
};


class ConfigDialog : public QDialog
{
Q_OBJECT

public:
   ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
     , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
     , const std::shared_ptr<SignersProvider> &signersProvider
     , const std::shared_ptr<SignContainer> &signContainer
     , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
     , QWidget* parent = nullptr);
   ~ConfigDialog() override;

   void popupNetworkSettings();

   enum class EncryptError
   {
      NoError,
      NoPrimaryWallet,
      NoEncryptionKey,
      EncryptError,
   };
   static QString encryptErrorStr(EncryptError error);
   using EncryptCb = std::function<void(EncryptError, const SecureBinaryData &data)>;
   static void encryptData(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<SignContainer> &signContainer, const SecureBinaryData &data, const EncryptCb &cb);
   static void decryptData(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<SignContainer> &signContainer, const SecureBinaryData &data, const EncryptCb &cb);

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
   static void getChatPrivKey(const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<SignContainer> &signContainer, const EncryptCb &cb);

   std::unique_ptr<Ui::ConfigDialog> ui_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<ArmoryServersProvider> armoryServersProvider_;
   std::shared_ptr<SignersProvider>       signersProvider_;
   std::vector<SettingsPage *>            pages_;
   ApplicationSettings::State             prevState_;
   std::shared_ptr<SignContainer>         signContainer_;

};

#endif
