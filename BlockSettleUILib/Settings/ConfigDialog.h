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
#include "../../Core/ArmoryServersProvider.h"
#include "Settings/SignersProvider.h"
#include "Wallets/SignContainer.h"

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

   [[deprecated]] virtual void init(const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
      , const std::shared_ptr<SignersProvider> &signersProvider
      , const std::shared_ptr<SignContainer> &signContainer
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr);
   virtual void init(const ApplicationSettings::State &);

   virtual void onSetting(int setting, const QVariant& value);

public slots:
   virtual void initSettings() {}
   virtual void display() = 0;
   virtual void reset() = 0;
   virtual void apply() = 0;

signals:
   void putSetting(ApplicationSettings::Setting, const QVariant&);
   void resetSettings(const std::vector<ApplicationSettings::Setting>&);
   void illformedSettings(bool illformed);

protected:
   ApplicationSettings::State    settings_;
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
   [[deprecated]] ConfigDialog(const std::shared_ptr<ApplicationSettings>& appSettings
     , const std::shared_ptr<ArmoryServersProvider> &armoryServersProvider
     , const std::shared_ptr<SignersProvider> &signersProvider
     , const std::shared_ptr<SignContainer> &signContainer
     , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
     , QWidget* parent = nullptr);
   ConfigDialog(QWidget* parent = nullptr);
   ~ConfigDialog() override;

   void popupNetworkSettings();

   void onSettingsState(const ApplicationSettings::State &);
   void onSetting(int setting, const QVariant& value);
   void onArmoryServers(const QList<ArmoryServer>&, int idxCur, int idxConn);
   void onSignerSettings(const QList<SignerHost>&, const std::string& ownKey, int idxCur);

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
   void onIllformedSettings(bool illformed);

signals:
   void reconnectArmory();
   void putSetting(ApplicationSettings::Setting, const QVariant&);
   void resetSettings(const std::vector<ApplicationSettings::Setting>&);
   void resetSettingsToState(const ApplicationSettings::State &);
   void setArmoryServer(int);
   void addArmoryServer(const ArmoryServer&);
   void delArmoryServer(int);
   void updArmoryServer(int, const ArmoryServer&);
   void setSigner(int);

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
