#ifndef __ARMORY_SERVERS_VEIW_MODEL_H__
#define __ARMORY_SERVERS_VEIW_MODEL_H__

#include <QAbstractTableModel>
#include <QMutex>
#include <memory>

#include "AuthAddress.h"
#include "AuthAddressManager.h"
#include "BinaryData.h"
#include "ApplicationSettings.h"
#include "ArmoryServersProvider.h"


class ArmoryServersViewModel : public QAbstractTableModel
{
public:
   ArmoryServersViewModel(const std::shared_ptr<ArmoryServersProvider>& serversProvider
                          , QObject *parent = nullptr);
   ~ArmoryServersViewModel() noexcept = default;

   ArmoryServersViewModel(const ArmoryServersViewModel&) = delete;
   ArmoryServersViewModel& operator = (const ArmoryServersViewModel&) = delete;

   ArmoryServersViewModel(ArmoryServersViewModel&&) = delete;
   ArmoryServersViewModel& operator = (ArmoryServersViewModel&&) = delete;

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
   void update();

private:
   std::shared_ptr<ArmoryServersProvider> serversProvider_;
   QList<ArmoryServer> servers_;

   enum ArmoryServersViewViewColumns : int
   {
      ColumnName,
      ColumnType,
      ColumnAddress,
      ColumnPort,
      ColumnKey
   };
};

#endif // __ARMORY_SERVERS_VEIW_MODEL_H__
