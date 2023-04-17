/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TX_INPUTS_MODEL_H
#define TX_INPUTS_MODEL_H

#include <QAbstractTableModel>
#include <QColor>
#include <QList>
#include <QObject>
#include <QVariant>
#include <QMutex>
#include <QWaitCondition>

#include "Address.h"
#include "BinaryData.h"
#include "TxClasses.h"

namespace spdlog {
   class logger;
}
class TxOutputsModel;


class QUTXO : public QObject
{
   Q_OBJECT
public:
   QUTXO(const UTXO& utxo, QObject* parent = nullptr)
      : QObject(parent), utxo_(utxo) {}

   struct Input {
      BinaryData  txHash;
      uint64_t    amount;
      uint32_t    txOutIndex;
   };
   QUTXO(const Input& input, QObject* parent = nullptr)
      : QObject(parent), input_(input) {}

   UTXO utxo() const { return utxo_; }
   Input input() const { return input_; }

private:
   UTXO  utxo_{};
   Input input_{};
};

class QUTXOList : public QObject
{
   Q_OBJECT
public:
   QUTXOList(const QList<QUTXO*>& data, QObject* parent = nullptr)
      : QObject(parent), data_(data)
   {}
   QList<QUTXO*> data() const { return data_; }

private:
   QList<QUTXO*> data_;
};

class TxInputsModel : public QAbstractTableModel
{
   Q_OBJECT    
   Q_PROPERTY(int rowCount READ rowCount NOTIFY rowCountChanged)

public:
   enum TableRoles { TableDataRole = Qt::UserRole + 1, HeadingRole, ColorRole,
                     SelectedRole, ExpandedRole, CanBeExpandedRole };
   TxInputsModel(const std::shared_ptr<spdlog::logger>&, TxOutputsModel*
      , QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   void clear();
   void addUTXOs(const std::vector<UTXO>&);
   void setTopBlock(uint32_t topBlock) { topBlock_ = topBlock; }

   struct Entry {
      bs::Address address;
      BinaryData  txId{};
      uint32_t    txOutIndex{ UINT32_MAX };
      uint64_t    amount{ 0 };
      bool  expanded{ false };
   };
   void addEntries(const std::vector<Entry>&);

   Q_PROPERTY(int nbTx READ nbTx NOTIFY selectionChanged)
   int nbTx() const { return nbTx_; }
   Q_PROPERTY(QString balance READ balance NOTIFY selectionChanged)
   QString balance() const { return QString::number(selectedBalance_ / BTCNumericTypes::BalanceDivider, 'f', 8); }

   Q_PROPERTY(QString fee READ fee WRITE setFee NOTIFY feeChanged)
   QString fee() const { return fee_; }
   void setFee(const QString& fee) { fee_ = fee; emit feeChanged(); }

   Q_INVOKABLE void toggle(int row);
   Q_INVOKABLE void toggleSelection(int row);
   Q_INVOKABLE QUTXOList* getSelection();
   Q_INVOKABLE QUTXOList* zcInputs() const;
   Q_INVOKABLE void updateAutoselection();

signals:
   void selectionChanged() const;
   void feeChanged() const;
   void rowCountChanged ();

private:
   QVariant getData(int row, int col) const;
   QColor dataColor(int row, int col) const;
   QList<QUTXO*> collectUTXOsFor(double amount);

private:
   enum Columns {ColumnAddress, ColumnTx, ColumnComment, ColumnBalance};

   std::shared_ptr<spdlog::logger>  logger_;
   TxOutputsModel* outsModel_{ nullptr };
   const QMap<int, QString> header_;
   std::map<bs::Address, std::vector<UTXO>>  utxos_;

   std::vector<Entry>   data_;
   std::set<std::pair<BinaryData, uint32_t>> selectionUtxos_;
   std::set<bs::Address> selectionAddresses_;
   bool selectionRoot_ {false};

   std::map<int, QList<QUTXO*>>   preSelected_;
   int nbTx_{ 0 };
   uint64_t  selectedBalance_{ 0 };
   QString fee_;
   uint32_t topBlock_{ 0 };
   double collectUTXOsForAmount_{ 0 };
};

#endif	// TX_INPUTS_MODEL_H
