#ifndef __SETTLEMENT_MONITOR_H__
#define __SETTLEMENT_MONITOR_H__

#include "ArmoryConnection.h"
#include "CoreWallet.h"
#include "ValidityFlag.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <memory>
#include <string>

#include <QObject>

namespace bs {

   enum class PayoutSignatureType : int;

   struct PayoutSigner
   {
      static void WhichSignature(const Tx &
         , const bs::Address &settlAddr
         , const BinaryData &buyAuthKey, const BinaryData &sellAuthKey
         , const std::shared_ptr<spdlog::logger> &
         , const std::shared_ptr<ArmoryConnection> &, std::function<void(bs::PayoutSignatureType)>);
   };

   class SettlementMonitor : public ArmoryCallbackTarget
   {
   public:
      SettlementMonitor(const std::shared_ptr<ArmoryConnection> &
         , const std::shared_ptr<spdlog::logger> &, const bs::Address &
         , const SecureBinaryData &buyAuthKey, const SecureBinaryData &sellAuthKey
         , const std::function<void()> &);

      ~SettlementMonitor() noexcept override;

      void checkNewEntries();

      int getPayinConfirmations() const { return payinConfirmations_; }
      int getPayoutConfirmations() const { return payoutConfirmations_; }

      PayoutSignatureType getPayoutSignerSide() const { return payoutSignedBy_; }
      void getPayinInput(const std::function<void(UTXO)> &, bool allowZC = true);

      static bs::core::wallet::TXSignRequest createPayoutTXRequest(UTXO
         , const bs::Address &recvAddr, float feePerByte, unsigned int topBlock);
      static UTXO getInputFromTX(const bs::Address &, const BinaryData &payinHash
         , const bs::XBTAmount& amount);
      static uint64_t getEstimatedFeeFor(UTXO input, const bs::Address &recvAddr
         , float feePerByte, unsigned int topBlock);

      int confirmedThreshold() const { return 6; }

   protected:
      // payin detected is sent on ZC and once it's get to block.
      // if payin is already on chain before monitor started, payInDetected will
      // emited only once
      virtual void onPayInDetected(int confirmationsNumber, const BinaryData &txHash) = 0;
      virtual void onPayOutDetected(int confirmationsNumber, PayoutSignatureType signedBy) = 0;
      virtual void onPayOutConfirmed(bs::PayoutSignatureType signedBy) = 0;

      void onNewBlock(unsigned int height, unsigned int branchHgt) override;
      void onZCReceived(const std::vector<bs::TXEntry> &) override;

   private:
      std::atomic_flag                          walletLock_ = ATOMIC_FLAG_INIT;
      std::shared_ptr<AsyncClient::BtcWallet>   rtWallet_;
      std::set<BinaryData>                      ownAddresses_;

      int payinConfirmations_ = -1;
      int payoutConfirmations_ = -1;

      bool payinInBlockChain_ = false;
      bool payoutConfirmedFlag_ = false;

      PayoutSignatureType payoutSignedBy_{};

   protected:
      std::shared_ptr<ArmoryConnection>   armoryPtr_;
      std::shared_ptr<spdlog::logger>     logger_;
      bs::Address                         settlAddress_;
      SecureBinaryData                    buyAuthKey_;
      SecureBinaryData                    sellAuthKey_;
      ValidityFlag validityFlag_;

   protected:
      void IsPayInTransaction(const ClientClasses::LedgerEntry &, std::function<void(bool)>) const;
      void IsPayOutTransaction(const ClientClasses::LedgerEntry &, std::function<void(bool)>) const;

      void CheckPayoutSignature(const ClientClasses::LedgerEntry &, std::function<void(PayoutSignatureType)>) const;

      void SendPayInNotification(const int confirmationsNumber, const BinaryData &txHash);
      void SendPayOutNotification(const ClientClasses::LedgerEntry &);
   };

   class SettlementMonitorCb : public SettlementMonitor
   {
   public:
      using onPayInDetectedCB = std::function<void (int, const BinaryData &)>;
      using onPayOutDetectedCB = std::function<void (int, PayoutSignatureType)>;
      using onPayOutConfirmedCB = std::function<void (PayoutSignatureType)>;

   public:
      SettlementMonitorCb(const std::shared_ptr<ArmoryConnection> &armory
         , const std::shared_ptr<spdlog::logger> &logger, const bs::Address &addr
         , const SecureBinaryData &buyAuthKey, const SecureBinaryData &sellAuthKey
         , const std::function<void()> &cbInited)
         : SettlementMonitor(armory, logger, addr, buyAuthKey, sellAuthKey, cbInited) {}
      ~SettlementMonitorCb() noexcept override;

      SettlementMonitorCb(const SettlementMonitorCb&) = delete;
      SettlementMonitorCb& operator = (const SettlementMonitorCb&) = delete;

      SettlementMonitorCb(SettlementMonitorCb&&) = delete;
      SettlementMonitorCb& operator = (SettlementMonitorCb&&) = delete;

      void start(const onPayInDetectedCB& onPayInDetected
         , const onPayOutDetectedCB& onPayOutDetected
         , const onPayOutConfirmedCB& onPayOutConfirmed);

      void stop();

   protected:
      void onPayInDetected(int confirmationsNumber, const BinaryData &txHash) override;
      void onPayOutDetected(int confirmationsNumber, PayoutSignatureType signedBy) override;
      void onPayOutConfirmed(PayoutSignatureType signedBy) override;

   private:
      onPayInDetectedCB    onPayInDetected_;
      onPayOutDetectedCB   onPayOutDetected_;
      onPayOutConfirmedCB  onPayOutConfirmed_;
   };

} //namespace bs

#endif // __SETTLEMENT_MONITOR_H__
