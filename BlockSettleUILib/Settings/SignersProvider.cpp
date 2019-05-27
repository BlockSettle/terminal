#include "SignersProvider.h"

#include <QDir>
#include <QStandardPaths>
#include "SignContainer.h"

SignersProvider::SignersProvider(const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent)
   : QObject(parent)
   , appSettings_(appSettings)
{
}

QList<SignerHost> SignersProvider::signers() const
{
   QStringList signersStrings = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);

   QList<SignerHost> signers;
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

   appSettings_->set(ApplicationSettings::signerRunMode, int(SignContainer::OpMode::Remote));
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

bool SignersProvider::add(const SignerHost &signer)
{
   if (signer.port < 1 || signer.port > USHRT_MAX) {
      return false;
   }
   if (signer.name.isEmpty()) {
      return false;
   }

   QList<SignerHost> signersData = signers();
   // check if signer with already exist
   for (const SignerHost &s : signersData) {
      if (s.name == signer.name) {
         return false;
      }
      if (s.address == signer.address && s.port == signer.port) {
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
   if (signer.port < 1 || signer.port > USHRT_MAX) {
      return false;
   }
   if (signer.name.isEmpty()) {
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
      if (s.address == signer.address && s.port == signer.port) {
         return false;
      }
   }

   QStringList signersTxt = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);

   signersTxt.replace(index, signer.toTextSettings());
   appSettings_->set(ApplicationSettings::remoteSigners, signersTxt);

   emit dataChanged();
   return true;
}

bool SignersProvider::remove(int index)
{
   QStringList signers = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);
   if (index >= 0 && index < signers.size()){
      signers.removeAt(index);
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

   QStringList signers = appSettings_->get<QStringList>(ApplicationSettings::remoteSigners);
   QString signerTxt = signers.at(index);
   SignerHost signer = SignerHost::fromTextSettings(signerTxt);
   signer.key = key;
   signers[index] = signer.toTextSettings();

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


