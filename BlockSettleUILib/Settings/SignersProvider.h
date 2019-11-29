/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SIGNERS_PROVIDER_H
#define SIGNERS_PROVIDER_H

#include <QObject>
#include "ApplicationSettings.h"
#include "BinaryData.h"


struct SignerHost
{
   QString name;
   QString address;
   int port = 0;
   QString key;

   bool operator ==(const SignerHost &other) const  {
      return address == other.address && port == other.port;
   }
   bool operator !=(const SignerHost &other) const  {
      return !(*this == other);
   }

   bool isValid() const {
      if (port < 1 || port > USHRT_MAX) {
         return false;
      }
      if (name.isEmpty()) {
         return false;
      }
      return true;
   }

   static SignerHost fromTextSettings(const QString &text) {
      SignerHost signer;
      if (text.split(QStringLiteral(":")).size() != 4) {
         return signer;
      }
      const QStringList &data = text.split(QStringLiteral(":"));
      signer.name = data.at(0);
      signer.address = data.at(1);
      signer.port = data.at(2).toInt();
      signer.key = data.at(3);

      return signer;
   }

   QString toTextSettings() const {
      return QStringLiteral("%1:%2:%3:%4")
            .arg(name)
            .arg(address)
            .arg(port)
            .arg(key);
   }

   std::string serverId() const {
      return address.toStdString() + ":" + std::to_string(port);
   }
};

class SignersProvider : public QObject
{
   Q_OBJECT
public:
   SignersProvider(const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent = nullptr);

   QList<SignerHost> signers() const;
   SignerHost getCurrentSigner() const;
   // Switch to local remote signer ("Local GUI Signer Mode")
   void switchToLocalFullGUI(const QString &host, const QString &port);

   int indexOfCurrent() const;   // index of server which set in ini file
   int indexOfConnected() const;   // index of server currently connected
   int indexOf(const QString &name) const;
   int indexOf(const SignerHost &server) const;
   int indexOfIpPort(const std::string &srvIPPort) const;

   bool currentSignerIsLocal();

   bool add(const SignerHost &signer);
   bool replace(int index, const SignerHost &signer);
   bool remove(int index);

   void addKey(const QString &address, int port, const QString &key);
   void addKey(const std::string &srvIPPort, const std::string &srvPubKey);

   SignerHost connectedSignerHost() const;
   void setConnectedSignerHost(const SignerHost &connectedSignerHost);

   void setupSigner(int index, bool needUpdate = true);

   std::string remoteSignerKeysDir() const;
   std::string remoteSignerKeysFile() const;
   BinaryData remoteSignerOwnKey() const;
signals:
   void dataChanged();
private:
   std::shared_ptr<ApplicationSettings> appSettings_;

   SignerHost connectedSignerHost_;  // latest connected signer
   mutable BinaryData remoteSignerOwnKey_;
};

#endif // SIGNERS_PROVIDER_H
