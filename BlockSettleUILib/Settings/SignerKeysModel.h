#ifndef __SIGNER_KEYS_MODEL_H__
#define __SIGNER_KEYS_MODEL_H__

#include <QAbstractTableModel>
#include <memory>

#include "AuthAddress.h"
#include "AuthAddressManager.h"
#include "BinaryData.h"
#include "ApplicationSettings.h"

struct SignerKey
{
   QString name;
   QString address;
   QString key;
};

class SignerKeysModel : public QAbstractTableModel
{
public:
   SignerKeysModel(const std::shared_ptr<ApplicationSettings>& appSettings
                          , QObject *parent = nullptr);
   ~SignerKeysModel() noexcept = default;

   SignerKeysModel(const SignerKeysModel&) = delete;
   SignerKeysModel& operator = (const SignerKeysModel&) = delete;

   SignerKeysModel(SignerKeysModel&&) = delete;
   SignerKeysModel& operator = (SignerKeysModel&&) = delete;

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   void addSignerPubKey(const SignerKey &key);
   void deleteSignerPubKey(int index);
   void editSignerPubKey(int index, const SignerKey &key);
   void saveSignerPubKeys(QList<SignerKey> signerKeys);

   QList<SignerKey> signerPubKeys() const;

public slots:
   void update();

private:
   std::shared_ptr<ApplicationSettings> appSettings_;
   QList<SignerKey> signerPubKeys_;

   enum ArmoryServersViewColumns : int
   {
      ColumnName,
      ColumnAddress,
      ColumnKey,
      ColumnsCount
   };
};

#endif // __SIGNER_KEYS_MODEL_H__
