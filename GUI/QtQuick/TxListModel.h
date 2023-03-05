/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TX_LIST_MODEL_H
#define TX_LIST_MODEL_H

#include <memory>
#include <QAbstractTableModel>
#include <QColor>
#include <QObject>
#include <QVariant>
#include "ArmoryConnection.h"
#include "TxInputsModel.h"
#include "Wallets/SignerDefs.h"

namespace spdlog {
   class logger;
}

class TxListModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles { TableDataRole = Qt::UserRole + 1, ColorRole, TxIdRole
      , RBFRole, NbConfRole };
   Q_ENUM(TableRoles)

   TxListModel(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   Q_PROPERTY(int nbTx READ nbTx NOTIFY nbTxChanged);
   int nbTx() const { return data_.size(); }

   Q_INVOKABLE bool exportCSVto(const QString&);

   void addRows(const std::vector<bs::TXEntry>&);
   void clear();
   void setTxComment(const std::string& txHash, const std::string& comment);
   void setDetails(const bs::sync::TXWalletDetails&);
   void setCurrentBlock(uint32_t);

signals:
   void nbTxChanged();

private:
   QVariant getData(int row, int col) const;
   QColor dataColor(int row, int col) const;
   QString walletName(int row) const;
   QString txType(int row) const;
   QString txFlag(int row) const;
   QString txId(int row) const;
   bool isRBF(int row) const;
   quint32 nbConf(int row) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const QStringList header_;
   std::vector<bs::TXEntry> data_;
   std::unordered_map<std::string, std::string> txComments_;
   std::map<int, bs::sync::TXWalletDetails>  txDetails_;
   uint32_t curBlock_;
};

class TxListForAddr : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles {
      TableDataRole = Qt::UserRole + 1, ColorRole
   };
   TxListForAddr(const std::shared_ptr<spdlog::logger>&, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   void addRows(const std::vector<bs::TXEntry>&);
   void clear();
   void setDetails(const std::vector<Tx>&);
   void setInputs(const std::vector<Tx>&);
   void setCurrentBlock(uint32_t);

   Q_PROPERTY(QString totalReceived READ totalReceived NOTIFY changed)
   QString totalReceived() const;
   Q_PROPERTY(QString totalSent READ totalSent NOTIFY changed)
   QString totalSent() const;
   Q_PROPERTY(QString balance READ balance NOTIFY changed)
   QString balance() const;
   Q_PROPERTY(int nbTx READ nbTx NOTIFY changed)
   int nbTx() const { return data_.size(); }

signals:
   void changed();

private:
   QString getData(int row, int col) const;
   QColor dataColor(int row, int col) const;
   QString txId(int row) const;
   int nbInputs(int row) const;
   int nbOutputs(int row) const;
   int txSize(int row) const;
   int64_t totalFees(int row) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const QStringList header_;
   std::vector<bs::TXEntry> data_;
   std::map<int, Tx> txs_;
   std::map<BinaryData, Tx>   inputs_;
   uint32_t curBlock_;
};


class TxInOutModel : public QAbstractTableModel
{
   Q_OBJECT
public:
   enum TableRoles {
      TableDataRole = Qt::UserRole + 1, ColorRole, TxHashRole
   };
   TxInOutModel(const std::vector<bs::sync::AddressDetails>& data, const QString& type, QObject* parent = nullptr);

   int rowCount(const QModelIndex & = QModelIndex()) const override;
   int columnCount(const QModelIndex & = QModelIndex()) const override;
   QVariant data(const QModelIndex& index, int role) const override;
   QHash<int, QByteArray> roleNames() const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

private:
   QString getData(int row, int col) const;
   QColor dataColor(int row, int col) const;

private:
   const QString type_;
   const QStringList header_;
   const std::vector<bs::sync::AddressDetails> data_;
};

namespace Transactions {
   Q_NAMESPACE
   enum Direction {
      Received = 0,
      Sent,
      Internal,
      Unknown
   };
   Q_ENUM_NS(Direction)
}

class QTxDetails : public QObject
{
   Q_OBJECT
public:
   QTxDetails(const std::shared_ptr<spdlog::logger>& logger, const BinaryData& txHash
      , QObject* parent = nullptr)
      : QObject(parent), logger_(logger), txHash_(txHash)
   {}

   void setDetails(const bs::sync::TXWalletDetails&);
   void setCurBlock(uint32_t);

   Q_PROPERTY(QString txId READ txId NOTIFY updated)
   QString txId() const { return QString::fromStdString(txHash_.toHexStr(true)); }
   Q_PROPERTY(QString virtSize READ virtSize NOTIFY updated)
   QString virtSize() const;
   Q_PROPERTY(QString nbConf READ nbConf NOTIFY newBlock)
   QString nbConf() const;
   Q_PROPERTY(QString nbInputs READ nbInputs NOTIFY updated)
   QString nbInputs() const;
   Q_PROPERTY(QString nbOutputs READ nbOutputs NOTIFY updated)
   QString nbOutputs() const;
   Q_PROPERTY(QString inputAmount READ inputAmount NOTIFY updated)
   QString inputAmount() const;
   Q_PROPERTY(QString outputAmount READ outputAmount NOTIFY updated)
   QString outputAmount() const;
   Q_PROPERTY(QString fee READ fee NOTIFY updated)
   QString fee() const;
   Q_PROPERTY(QString feePerByte READ feePerByte NOTIFY updated)
   QString feePerByte() const;
   Q_PROPERTY(quint32 height READ height NOTIFY updated)
   quint32 height() const;

   Q_PROPERTY(TxInOutModel* inputs READ inputs NOTIFY updated)
   TxInOutModel* inputs() const { return inputsModel_; }
   Q_PROPERTY(TxInOutModel* outputs READ outputs NOTIFY updated)
   TxInOutModel* outputs() const { return outputsModel_; }
   Q_PROPERTY(TxInputsModel* ownInputs READ ownInputs NOTIFY updated)
   TxInputsModel* ownInputs() const { return ownInputs_; }
   Q_PROPERTY(TxInputsModel* ownOutputs READ ownOutputs NOTIFY updated)
   TxInputsModel* ownOutputs() const { return ownOutputs_; }
   std::vector<std::pair<bs::Address, double>> outputData() const;

   bs::sync::Transaction::Direction direction() const;

signals:
   void updated();
   void newBlock();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   const BinaryData txHash_;
   bs::sync::TXWalletDetails details_;
   TxInOutModel* inputsModel_{ nullptr };
   TxInOutModel* outputsModel_{ nullptr };
   TxInputsModel* ownInputs_{ nullptr };
   TxInputsModel* ownOutputs_{ nullptr };
   std::vector<TxInputsModel::Entry> outputs_;
   uint32_t curBlock_{ 0 };
};

#endif	// TX_LIST_MODEL_H
