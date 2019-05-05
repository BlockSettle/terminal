#include <QFont>
#include <QTreeView>
#include "SignersModel.h"

SignerKeysModel::SignerKeysModel(const std::shared_ptr<SignersProvider>& signersProvider
                                               , QObject *parent)
   : QAbstractTableModel(parent)
   , signersProvider_(signersProvider)
{
   update();
   connect(signersProvider.get(), &SignersProvider::dataChanged, this, &SignerKeysModel::update);
}

int SignerKeysModel::columnCount(const QModelIndex&) const
{
   return static_cast<int>(SignerKeysModel::ColumnsCount);
}

int SignerKeysModel::rowCount(const QModelIndex&) const
{
   return signers_.size();
}

QVariant SignerKeysModel::data(const QModelIndex &index, int role) const
{
   if (index.row() >= signers_.size()) return QVariant();
   SignerHost signerHost = signers_.at(index.row());

   if (role == Qt::DisplayRole) {
      switch (index.column()) {
      case ColumnName:
         return signerHost.name;
      case ColumnAddress:
         return signerHost.address;
      case ColumnPort:
         return signerHost.port;
      case ColumnKey:
         return signerHost.key;
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
      switch(static_cast<SignersViewColumns>(section)) {
      case SignersViewColumns::ColumnName:
         return tr("Name");
      case SignersViewColumns::ColumnAddress:
         return tr("Address");
      case SignersViewColumns::ColumnPort:
         return tr("Port");
      case SignersViewColumns::ColumnKey:
         return tr("Key");
      default:
         return QVariant();
      }
   }

   return QVariant();
}

//void SignerKeysModel::addSignerPubKey(const SignerKey &key)
//{
//   QList<SignerKey> signerKeysCopy = signerPubKeys_;
//   signerKeysCopy.append(key);
//   saveSignerPubKeys(signerKeysCopy);
//   update();
//}

//void SignerKeysModel::deleteSignerPubKey(int index)
//{
//   QList<SignerKey> signerKeysCopy = signerPubKeys_;
//   signerKeysCopy.removeAt(index);
//   saveSignerPubKeys(signerKeysCopy);
//   update();
//}

//void SignerKeysModel::editSignerPubKey(int index, const SignerKey &key)
//{
//   if (index < 0 || index > signerPubKeys_.size()) {
//      return;
//   }
//   QList<SignerKey> signerKeysCopy = signerPubKeys_;
//   signerKeysCopy[index] = key;

//   saveSignerPubKeys(signerKeysCopy);
//   update();
//}

//void SignerKeysModel::saveSignerPubKeys(QList<SignerKey> signerKeys)
//{
//   QStringList signerKeysString;
//   for (const SignerKey &key : signerKeys) {
//      QString s = QStringLiteral("%1:%2:%3:%4")
//            .arg(key.name)
//            .arg(key.address)
//            .arg(key.port)
//            .arg(key.key);
//      signerKeysString.append(s);
//   }

//   appSettings_->set(ApplicationSettings::remoteSigners, signerKeysString);
//}

void SignerKeysModel::update()
{
   beginResetModel();
   signers_ = signersProvider_->signers();
   endResetModel();
}

//QList<SignerKey> SignerKeysModel::signerPubKeys() const
//{
//   return signerPubKeys_;
//}



