/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#include <memory>

#include <QObject>

namespace spdlog {
   class logger;
}

namespace qtquick_gui
{

struct WalletInfo
{
   QString name;
   QString walletId;
   QString groups;
   QString walletType;
   quint32 generatedAddresses;
   bool isHardware;
   bool isWatchingOnly;
};

class WalletPropertiesVM: public QObject
{
   Q_OBJECT
   Q_PROPERTY(QString walletName               READ walletName               NOTIFY changed)
   Q_PROPERTY(QString walletId                 READ walletId                 NOTIFY changed)
   Q_PROPERTY(QString walletGroups             READ walletGroups             NOTIFY changed)
   Q_PROPERTY(QString walletType               READ walletType               NOTIFY changed)
   Q_PROPERTY(quint32 walletGeneratedAddresses READ walletGeneratedAddresses NOTIFY changed)
   Q_PROPERTY(quint32 walletUsedAddresses      READ walletUsedAddresses    NOTIFY changed)
   Q_PROPERTY(quint32 walletAvailableUtxo      READ walletAvailableUtxo      NOTIFY changed)
   Q_PROPERTY(bool isHardware                  READ isHardware               NOTIFY changed)
   Q_PROPERTY(bool isWatchingOnly              READ isWatchingOnly           NOTIFY changed)
   Q_PROPERTY(QStringList seed                 READ seed                     NOTIFY seedChanged)

public:
   WalletPropertiesVM(const std::shared_ptr<spdlog::logger> & logger, QObject* parent = nullptr);

   void setWalletInfo(const WalletInfo& info);
   void setWalletSeed(const std::string& walletId, const std::string& seed);
   void setNbUsedAddrs(const std::string& walletId, uint32_t nb);
   void setNbUTXOs(const std::string& walletId, uint32_t nb);

   const QString& walletName() const;
   const QString& walletId() const;
   const QString& walletGroups() const;
   const QString& walletType() const;
   quint32 walletGeneratedAddresses() const;
   quint32 walletUsedAddresses() const;
   quint32 walletAvailableUtxo() const;
   bool isHardware() const;
   bool isWatchingOnly() const;

   const QStringList& seed() const;
   
signals:
   void changed();
   void seedChanged();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   WalletInfo  info_;
   uint32_t    nbUsedAddrs_{ 0 };
   uint32_t    nbUTXOs_{ 0 };
   QStringList seed_;
};

}
