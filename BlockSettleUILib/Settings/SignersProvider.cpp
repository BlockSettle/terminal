/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignersProvider.h"

#include <QDir>
#include <QStandardPaths>
#include "SignContainer.h"
#include "SystemFileUtils.h"

SignersProvider::SignersProvider(const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent)
   : QObject(parent)
   , appSettings_(appSettings)
{
}

QList<SignerHost> SignersProvider::signers() const
{
   QStringList signersStrings = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);

   QList<SignerHost> signers;

   // #1 add Headless signer
   SignerHost headlessSigner;
   headlessSigner.name = tr("Local Headless");
   headlessSigner.address = tr("-");
   headlessSigner.port = 0;
   headlessSigner.key = tr("Auto");

   signers.append(headlessSigner);

   for (const QString &s : signersStrings) {
      signers.append(SignerHost::fromTextSettings(s));
   }

   return signers;
}

SignerHost SignersProvider::getCurrentSigner() const
{
   const int index = appSettings_->get<int>(ApplicationSettings::signerIndex);
   QList<SignerHost> signersData = signers();

   if (index < 0 || index >= signersData.size()) {
      return SignerHost();
   }

   return signersData.at(index);
}

void SignersProvider::switchToLocalFullGUI(const QString &host, const QString &port)
{
   SignerHost local;
   local.name = appSettings_->localSignerDefaultName();
   local.address = host;
   local.port = port.toInt();

   int localIndex = indexOf(local);
   if (localIndex < 0) {
      add(local);
      localIndex = indexOf(local);
      if (localIndex < 0) {
         return;
      }
   }

   setupSigner(localIndex, true);
}

int SignersProvider::indexOfCurrent() const
{
   int index = appSettings_->get<int>(ApplicationSettings::signerIndex);
   QList<SignerHost> signersData = signers();
   if (index < 0 || index >= signers().size()) {
      return -1;
   }
   return index;
}

int SignersProvider::indexOfConnected() const
{
   return indexOf(connectedSignerHost_);
}

int SignersProvider::indexOf(const QString &name) const
{
   // naive implementation
   QList<SignerHost> s = signers();
   for (int i = 0; i < s.size(); ++i) {
      if (s.at(i).name == name) {
         return i;
      }
   }
   return -1;
}

int SignersProvider::indexOf(const SignerHost &server) const
{
   return signers().indexOf(server);
}

int SignersProvider::indexOfIpPort(const std::string &srvIPPort) const
{
   QString ipPort = QString::fromStdString(srvIPPort);
   QStringList ipPortList = ipPort.split(QStringLiteral(":"));
   if (ipPortList.size() != 2) {
      return -1;
   }

   for (int i = 0; i < signers().size(); ++i) {
      if (signers().at(i).address == ipPortList.at(0) && signers().at(i).port == ipPortList.at(1).toInt()) {
         return i;
      }
   }
   return -1;
}

bool SignersProvider::currentSignerIsLocal()
{
   return indexOfCurrent() == 0;
}

bool SignersProvider::add(const SignerHost &signer)
{
   if (!signer.isValid()) {
      return false;
   }

   QList<SignerHost> signersData = signers();
   // check if signer with already exist
   for (const SignerHost &s : signersData) {
      if (s.name == signer.name) {
         return false;
      }
      if (s == signer) {
         return false;
      }
   }

   QStringList signersTxt = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);

   signersTxt.append(signer.toTextSettings());
   appSettings_->set(ApplicationSettings::remoteSigners, signersTxt);
   emit dataChanged();
   return true;
}

bool SignersProvider::replace(int index, const SignerHost &signer)
{
   if (index == 0 || !signer.isValid()) {
      return false;
   }

   QList<SignerHost> signersData = signers();
   if (index >= signersData.size()) {
      return false;
   }

   // check if signer with already exist
   for (int i = 0; i < signersData.size(); ++i) {
      if (i == index) continue;

      const SignerHost &s = signersData.at(i);
      if (s.name == signer.name) {
         return false;
      }
      if (s == signer) {
         return false;
      }
   }

   QStringList signersTxt = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);
   int settingsIndex = index - 1;

   signersTxt.replace(settingsIndex, signer.toTextSettings());
   appSettings_->set(ApplicationSettings::remoteSigners, signersTxt);

   emit dataChanged();
   return true;
}

bool SignersProvider::remove(int index)
{
   QStringList signers = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);
   if (index - 1 >= 0 && index - 1 < signers.size()){
      signers.removeAt(index - 1);
      appSettings_->set(ApplicationSettings::remoteSigners, signers);
      emit dataChanged();
      return true;
   }
   else {
      return false;
   }
}

void SignersProvider::addKey(const QString &address, int port, const QString &key)
{
   int index = -1;
   for (int i = 0; i < signers().size(); ++i) {
      if (signers().at(i).address == address && signers().at(i).port == port) {
         index = i;
         break;
      }
   }

   if (index == -1){
      return;
   }

   int settingsIndex = index - 1;

   QStringList signers = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);
   QString signerTxt = signers.at(settingsIndex);
   SignerHost signer = SignerHost::fromTextSettings(signerTxt);
   signer.key = key;
   signers[settingsIndex] = signer.toTextSettings();

   appSettings_->set(ApplicationSettings::remoteSigners, signers);

   // update key for current signer
   connectedSignerHost_.key = key;

   emit dataChanged();
}

void SignersProvider::addKey(const std::string &srvIPPort, const std::string &srvPubKey)
{
   QString ipPort = QString::fromStdString(srvIPPort);
   QStringList ipPortList = ipPort.split(QStringLiteral(":"));
   if (ipPortList.size() == 2) {
      addKey(ipPortList.at(0)
             , ipPortList.at(1).toInt()
             , QString::fromStdString(srvPubKey));
   }
}

SignerHost SignersProvider::connectedSignerHost() const
{
   return connectedSignerHost_;
}

void SignersProvider::setConnectedSignerHost(const SignerHost &connectedSignerHost)
{
   connectedSignerHost_ = connectedSignerHost;
}

void SignersProvider::setupSigner(int index, bool needUpdate)
{
   QList<SignerHost> signerList = signers();
   if (index >= 0 && index < signerList.size()) {
      appSettings_->set(ApplicationSettings::signerIndex, index);

      if (needUpdate)
         emit dataChanged();
   }
}

std::string SignersProvider::remoteSignerKeysDir() const
{
   return SystemFilePaths::appDataLocation();
}

std::string SignersProvider::remoteSignerKeysFile() const
{
   return "client.peers";
}

BinaryData SignersProvider::remoteSignerOwnKey() const
{
   if (remoteSignerOwnKey_.isNull()) {
      remoteSignerOwnKey_ = ZmqBIP15XDataConnection::getOwnPubKey(remoteSignerKeysDir(), remoteSignerKeysFile());
   }
   return remoteSignerOwnKey_;
}
