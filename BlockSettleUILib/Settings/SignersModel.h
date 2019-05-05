#ifndef __SIGNERS_MODEL_H__
#define __SIGNERS_MODEL_H__

#include <QAbstractTableModel>
#include <memory>

#include "BinaryData.h"
#include "ApplicationSettings.h"
#include "SignersProvider.h"

class SignerKeysModel : public QAbstractTableModel
{
public:
   SignerKeysModel(const std::shared_ptr<SignersProvider> &signersProvider
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

//   void addSignerPubKey(const SignerKey &key);
//   void deleteSignerPubKey(int index);
//   void editSignerPubKey(int index, const SignerKey &key);
//   void saveSignerPubKeys(QList<SignerKey> signerKeys);

//   QList<SignerKey> signerPubKeys() const;

public slots:
   void update();

private:
   std::shared_ptr<SignersProvider> signersProvider_;
   QList<SignerHost> signers_;
   bool singleColumnMode_ = false;

   enum SignersViewColumns : int
   {
      ColumnName,
      ColumnAddress,
      ColumnPort,
      ColumnKey,
      ColumnsCount
   };
};

#endif // __SIGNERS_MODEL_H__
