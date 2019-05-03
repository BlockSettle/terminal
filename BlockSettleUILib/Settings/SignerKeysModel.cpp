#include <QFont>
#include <QTreeView>
#include "SignerKeysModel.h".h"
#include "EncryptionUtils.h"
#include "ArmoryConnection.h"

SignerKeysModel::SignerKeysModel(const std::shared_ptr<ApplicationSettings> &appSettings
                                               , QObject *parent)
   : QAbstractTableModel(parent)
   , appSettings_(appSettings)
{
   update();
   connect(appSettings.get(), &ApplicationSettings::settingChanged, this, [this](int setting, QVariant value){
      if (setting == ApplicationSettings::remoteSignerKeys) {
         update();
      }
   });
}

int SignerKeysModel::columnCount(const QModelIndex&) const
{
   return static_cast<int>(SignerKeysModel::ColumnsCount);
}

int SignerKeysModel::rowCount(const QModelIndex&) const
{
   return signerPubKeys_.size();
}

QVariant SignerKeysModel::data(const QModelIndex &index, int role) const
{
   if (index.row() >= signerPubKeys_.size()) return QVariant();
   SignerKey signerKey = signerPubKeys_.at(index.row());

   if (role == Qt::DisplayRole) {
      switch (index.column()) {
      case ColumnName:
         return signerKey.name;
      case ColumnAddress:
         return signerKey.address;
      case ColumnKey:
         return signerKey.key;
      default:
         break;
      }
   }
   return QVariant();
}

QVariant SignerKeysModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role == Qt::DisplayRole) {
      switch(static_cast<ArmoryServersViewColumns>(section)) {
      case ArmoryServersViewColumns::ColumnName:
         return tr("Name");
      case ArmoryServersViewColumns::ColumnAddress:
         return tr("Address");
      case ArmoryServersViewColumns::ColumnKey:
         return tr("Key");
      default:
         return QVariant();
      }
   }

   return QVariant();
}

void SignerKeysModel::addSignerPubKey(const SignerKey &key)
{
   QList<SignerKey> signerKeysCopy = signerPubKeys_;
   signerKeysCopy.append(key);
   saveSignerPubKeys(signerKeysCopy);
   update();
}

void SignerKeysModel::deleteSignerPubKey(int index)
{
   QList<SignerKey> signerKeysCopy = signerPubKeys_;
   signerKeysCopy.removeAt(index);
   saveSignerPubKeys(signerKeysCopy);
   update();
}

void SignerKeysModel::editSignerPubKey(int index, const SignerKey &key)
{
   if (index < 0 || index > signerPubKeys_.size()) {
      return;
   }
   QList<SignerKey> signerKeysCopy = signerPubKeys_;
   signerKeysCopy[index] = key;

   saveSignerPubKeys(signerKeysCopy);
   update();
}

void SignerKeysModel::saveSignerPubKeys(QList<SignerKey> signerKeys)
{
   QStringList signerKeysString;
   for (const SignerKey &key : signerKeys) {
      QString s = QStringLiteral("%1:%2:%3").arg(key.name).arg(key.address).arg(key.key);
      signerKeysString.append(s);
   }

   appSettings_->set(ApplicationSettings::remoteSignerKeys, signerKeysString);
}

void SignerKeysModel::update()
{
   beginResetModel();

   signerPubKeys_.clear();
   QStringList keysString = appSettings_->get(ApplicationSettings::remoteSignerKeys).toStringList();

   SignerKey signerKey;
   for (const QString &s: keysString) {
      const QStringList &ks = s.split(QStringLiteral(":"));
      if (ks.size() != 3) {
         continue;
      }
      signerKey.name = ks.at(0);
      signerKey.address = ks.at(1);
      signerKey.key = ks.at(2);

      signerPubKeys_.append(signerKey);
   }

   endResetModel();
}

QList<SignerKey> SignerKeysModel::signerPubKeys() const
{
   return signerPubKeys_;
}



