#ifndef __BS_QWALLETINFO_H__
#define __BS_QWALLETINFO_H__

#include <memory>
#include <QObject>
#include "HDWallet.h"
#include "MetaData.h"
#include "WalletEncryption.h"
#include "AutheIDClient.h"

class WalletsManager;
class AuthSignWalletObject;

namespace bs {
namespace wallet {

/// define Q_NAMESPACE to provide Q_ENUM_NS macro
/// QEncryptionType and QNetworkType exported to qml
Q_NAMESPACE

enum class QEncryptionType : uint8_t {
   Unencrypted,
   Password,
   Auth
};
Q_ENUM_NS(QEncryptionType)

enum QNetworkType
{
   first,
   MainNet = first,
   TestNet,
   RegTest,
   last,
   Invalid = last
};
Q_ENUM_NS(QNetworkType)

QNetworkType toQNetworkType(NetworkType netType);
NetworkType fromQNetworkType(bs::wallet::QNetworkType netType);


// PasswordData::password might be either binary ot plain text depends of wallet encryption type
// for QEncryptionType::Password - it's plain text
// for QEncryptionType::Auth - it's binary
// textPassword and binaryPassword properties provides Qt interfaces for PasswordData::password usable in QML
// password size limited to 32 bytes
class QPasswordData : public QObject, public PasswordData {
   Q_OBJECT

   Q_PROPERTY(QString textPassword READ q_textPassword WRITE q_setTextPassword NOTIFY passwordChanged)
   Q_PROPERTY(SecureBinaryData binaryPassword READ q_binaryPassword WRITE q_setBinaryPassword NOTIFY passwordChanged)
   Q_PROPERTY(QEncryptionType encType READ q_encType WRITE q_setEncType NOTIFY encTypeChanged)
   Q_PROPERTY(QString encKey READ q_encKey WRITE q_setEncKey NOTIFY encKeyChanged)

public:
   QPasswordData(QObject *parent = nullptr) : QObject(parent), PasswordData() {}
   QString q_textPassword() { return QString::fromStdString(password.toBinStr()); }
   SecureBinaryData q_binaryPassword() { return password; }
   QEncryptionType q_encType() { return static_cast<QEncryptionType>(encType); }
   QString q_encKey() { return QString::fromStdString(encKey.toBinStr()); }

   void q_setTextPassword(const QString &pw) { password =  SecureBinaryData(pw.toStdString());
                                             emit passwordChanged(); }
   void q_setBinaryPassword(const SecureBinaryData &data) { password =  data;
                                             emit passwordChanged(); }
   void q_setEncType(QEncryptionType e) { encType =  static_cast<EncryptionType>(e);
                                             emit encTypeChanged(e); }
   void q_setEncKey(const QString &e) { encKey =  SecureBinaryData(e.toStdString());
                                      emit encKeyChanged(e); }

signals:
   void passwordChanged();
   void encTypeChanged(QEncryptionType);
   void encKeyChanged(QString);
};


/// wrapper on bs::wallet::Seed enables using this type in QML
/// Seed class may operate with plain seed key (seed_) and secured data(privKey_)
class QSeed : public QObject, public Seed
{
   Q_OBJECT

   Q_PROPERTY(QString part1 READ part1)
   Q_PROPERTY(QString part2 READ part2)
   Q_PROPERTY(QString walletId READ walletId)
   Q_PROPERTY(bs::wallet::QNetworkType networkType READ networkType)
public:
   // empty seed with default constructor
   // required for qml
   QSeed(QObject* parent = nullptr)
      : QObject(parent), Seed(NetworkType::Invalid) {}

   // for qml
   QSeed(bool isTestNet)
      : Seed(isTestNet ? NetworkType::TestNet : NetworkType::MainNet
                         , SecureBinaryData().GenerateRandom(32)) {}

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
   QString walletId() { return QString::fromStdString(bs::hd::Node(*this).getId()); }

   QString lastError() const;

   QNetworkType networkType() { return toQNetworkType(Seed::networkType()); }
private:
   QString lastError_;
};

} //namespace wallet


namespace hd {

class WalletInfo : public QObject
{
   Q_OBJECT

   Q_PROPERTY(QString walletId READ walletId WRITE setWalletId NOTIFY walletChanged)
   Q_PROPERTY(QString name READ name WRITE setName NOTIFY walletChanged)
   Q_PROPERTY(QString desc READ desc WRITE setDesc NOTIFY walletChanged)
   Q_PROPERTY(QStringList encKeys READ encKeys WRITE setEncKeys NOTIFY walletChanged)

   // currently we using only single encription type for whole wallet
   Q_PROPERTY(bs::wallet::QEncryptionType encType READ encType NOTIFY walletChanged)

   //   Q_PROPERTY(QString rootId READ rootId WRITE setRootId NOTIFY rootIdChanged)
public:
   WalletInfo(QObject *parent = nullptr) : QObject(parent) {}
   WalletInfo(const std::shared_ptr<WalletsManager> &, const QString &walletId, QObject *parent = nullptr);

   WalletInfo(const WalletInfo &other);
   WalletInfo& operator= (const WalletInfo &other);

   static WalletInfo fromDigitalBackup(const QString &filename);
   void initFromWallet(const bs::Wallet *, const std::string &rootId = {});
   void initFromRootWallet(const std::shared_ptr<bs::hd::Wallet> &);

   QString walletId() const { return walletId_; }
   void setWalletId(const QString &walletId);

   QString desc() const { return desc_; }
   void setDesc(const QString &);

   QString name() const { return name_; }
   void setName(const QString &);

   QString rootId() const { return rootId_; }
   void setRootId(const QString &);

   QList<QString> encKeys() const { return encKeys_; }
   void setEncKeys(const QList<QString> &encKeys);

   Q_INVOKABLE QList<bs::wallet::QEncryptionType> encTypes() const { return encTypes_; }
   void setEncTypes(const QList<bs::wallet::QEncryptionType> &encTypes);

   // currently we supports only sigle enc type for whole wallet: either Password or eID Auth
   // this function returns encType based on first passwordDataList_ value
   bs::wallet::QEncryptionType encType();

   // currently we supports only sigle account for whole wallet, thus email stored in encKeys_.at(0)
   Q_INVOKABLE QString email();

signals:
   void walletChanged();


private:
   QString    walletId_;
   QString    rootId_;
   QString    name_, desc_;
   QList<QString> encKeys_;
   QList<bs::wallet::QEncryptionType> encTypes_;
   std::shared_ptr<WalletsManager> walletsManager_;
};


}  //namespace hd
}  //namespace bs


#endif // __BS_QWALLETINFO_H__
