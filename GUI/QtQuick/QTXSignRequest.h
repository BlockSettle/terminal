/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef Q_TX_SIGN_REQUEST_H
#define Q_TX_SIGN_REQUEST_H

#include <QAbstractTableModel>
#include <QObject>
#include <QVariant>
#include "BinaryData.h"
#include "CoreWallet.h"

namespace spdlog {
   class logger;
}

class QTXSignRequest : public QObject
{
   Q_OBJECT
public:
   struct Recipient {
      const bs::Address address;
      double            amount;
   };

   QTXSignRequest(QObject* parent = nullptr);
   bs::core::wallet::TXSignRequest txReq() const { return txReq_; }
   void setTxSignReq(const bs::core::wallet::TXSignRequest&);
   void setError(const QString&);
   void addDummyUTXO(const UTXO& utxo) { dummyUTXOs_.push_back(utxo); }
   std::vector<UTXO> dummyUTXOs() const { return dummyUTXOs_; }

   void setHWW(bool hww)
   {
      if (isHWW_ != hww) {
         isHWW_ = hww;
         emit hwwChanged();
      }
   }
   void setHWWready()
   {
      isHWWready_ = true;
      emit hwwReady();
   }

   Q_PROPERTY(QStringList outputAddresses READ outputAddresses NOTIFY txSignReqChanged)
   QStringList outputAddresses() const;
   Q_PROPERTY(QString outputAmount READ outputAmount NOTIFY txSignReqChanged)
   QString outputAmount() const;
   Q_PROPERTY(QString inputAmount READ inputAmount NOTIFY txSignReqChanged)
   QString inputAmount() const;
   Q_PROPERTY(QString returnAmount READ returnAmount NOTIFY txSignReqChanged)
   QString returnAmount() const;
   Q_PROPERTY(QString fee READ fee NOTIFY txSignReqChanged)
   QString fee() const;
   Q_PROPERTY(quint32 txSize READ txSize NOTIFY txSignReqChanged)
   quint32 txSize() const;
   Q_PROPERTY(QString feePerByte READ feePerByte NOTIFY txSignReqChanged)
   QString feePerByte() const;
   Q_PROPERTY(QString errorText READ errorText NOTIFY error)
   QString errorText() const { return error_; }
   Q_PROPERTY(bool isValid READ isValid NOTIFY txSignReqChanged)
   bool isValid() const { return (error_.isEmpty() && txReq_.isValid()); }
   Q_PROPERTY(QString maxAmount READ outputAmount NOTIFY txSignReqChanged)
   Q_PROPERTY(bool isHWW READ isHWW NOTIFY hwwChanged)
   bool isHWW() const { return isHWW_; }
   Q_PROPERTY(bool isHWWready READ isHWWready NOTIFY hwwReady)
   bool isHWWready() const { return isHWWready_; }
   Q_PROPERTY(bool hasError READ hasError NOTIFY error)
   bool hasError() const { return !error_.isEmpty(); }

signals:
   void txSignReqChanged();
   void hwwChanged();
   void hwwReady();
   void error();

private:
   bs::core::wallet::TXSignRequest txReq_{};
   QString  error_;
   std::vector<UTXO> dummyUTXOs_;
   bool  isHWW_{ false };
   bool  isHWWready_{ false };
};

#endif	// Q_TX_SIGN_REQUEST_H
