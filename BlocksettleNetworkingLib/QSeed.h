/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BS_QSEED_H__
#define __BS_QSEED_H__

#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "WalletEncryption.h"

namespace bs {
namespace wallet {

/// wrapper on bs::wallet::Seed enables using this type in QML
/// Seed class may operate with plain seed key (seed_) and secured data(privKey_)
class QSeed : public QObject, public bs::core::wallet::Seed
{
   Q_OBJECT

   Q_PROPERTY(QString part1 READ part1)
   Q_PROPERTY(QString part2 READ part2)
   Q_PROPERTY(QString walletId READ walletId CONSTANT) //see below
   Q_PROPERTY(int networkType READ networkType)
public:
   enum QNetworkType {
      first,
      MainNet = first,
      TestNet,
      RegTest,
      last,
      Invalid = last
   };
   Q_ENUM(QNetworkType)

   static QNetworkType toQNetworkType(NetworkType netType);
   static NetworkType fromQNetworkType(QNetworkType netType);

   // empty seed with default constructor
   // required for qml
   QSeed(QObject* parent = nullptr)
      : QObject(parent), Seed(NetworkType::Invalid) {}

   // for qml
   QSeed(bool isTestNet)
      : Seed(
         CryptoPRNG::generateRandom(32),
         isTestNet ? NetworkType::TestNet : NetworkType::MainNet)
   {}

   QSeed(const QString &seed, QNetworkType netType)
      : Seed(seed.toStdString(), fromQNetworkType(netType)) {}

   // copy constructors and operator= uses parent implementation
   QSeed(const Seed &seed) : Seed(seed){}
   QSeed(const QSeed &other) : QSeed(static_cast<Seed>(other)) {}
   QSeed& operator= (const QSeed &other) { Seed::operator=(other); return *this;}

   static QSeed fromPaperKey(const QString &key, QNetworkType netType);
   static QSeed fromDigitalBackup(const QString &filename, QNetworkType netType);

   QString part1() const { return QString::fromStdString(toEasyCodeChecksum().part1); }
   QString part2() const { return QString::fromStdString(toEasyCodeChecksum().part2); }
   QString walletId() const { return QString::fromStdString(getWalletId()); }

   QString lastError() const;

   QNetworkType networkType() { return toQNetworkType(Seed::networkType()); }
private:
   QString lastError_;
};

} //namespace wallet
}  //namespace bs


#endif // __BS_QSEED_H__
