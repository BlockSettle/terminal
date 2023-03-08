/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef QT_QUICK_ADAPTER_H
#define QT_QUICK_ADAPTER_H

#include <set>

#include <QObject>

#include "Address.h"
#include "AddressListModel.h"
#include "ApiAdapter.h"
#include "ApplicationSettings.h"
#include "hwdevicemodel.h"
#include "ThreadSafeClasses.h"
#include "TxInputsModel.h"
#include "TxInputsSelectedModel.h"
#include "TxListModel.h"
#include "UiUtils.h"
#include "Wallets/SignContainer.h"
#include "viewmodels/WalletPropertiesVM.h"

#include "common.pb.h"

namespace bs {
   namespace gui {
      namespace qt {
         class MainWindow;
      }
   }
}
namespace BlockSettle {
   namespace HW {
      class DeviceMgrMessage_Devices;
      class DeviceMgrMessage_SignTxResponse;
   }
   namespace Terminal {
      class AssetsMessage_Balance;
      class AssetsMessage_SubmittedAuthAddresses;
      class BsServerMessage_LoginResult;
      class BsServerMessage_Orders;
      class BsServerMessage_StartLoginResult;
      class MatchingMessage_LoggedIn;
      class Quote;
      class QuoteCancelled;
      class IncomingRFQ;
      class MatchingMessage_Order;
      class MktDataMessage_Prices;
      class SettlementMessage_FailedSettlement;
      class SettlementMessage_MatchedQuote;
      class SettlementMessage_PendingSettlement;
      class SettlementMessage_SettlementIds;
      class SettingsMessage_ArmoryServers;
      class SettingsMessage_SettingsResponse;
      class SettingsMessage_SignerServers;
      class SignerMessage_WalletSeed;
   }
}
class ArmoryServersModel;
class BSTerminalSplashScreen;
class FeeSuggestionModel;
class HwDeviceModel;
class QQmlContext;
class QmlWalletsList;
class QTxDetails;
class QTXSignRequest;
class TxInputsModel;
class TxOutputsModel;
class WalletBalancesModel;

class QtQuickAdapter : public QObject, public ApiBusAdapter, public bs::MainLoopRuner
{
   Q_OBJECT
   friend class GuiThread;
public:
   QtQuickAdapter(const std::shared_ptr<spdlog::logger> &);
   ~QtQuickAdapter() override;

   bs::message::ProcessingResult process(const bs::message::Envelope &) override;
   bool processBroadcast(const bs::message::Envelope&) override;

   Users supportedReceivers() const override { return { user_ }; }
   std::string name() const override { return "QtQuick"; }

   void run(int &argc, char **argv) override;

   Q_PROPERTY(QStringList txWalletsList READ txWalletsList NOTIFY walletsListChanged)
   QStringList txWalletsList() const;
   Q_PROPERTY(QStringList txTypesList READ txTypesList CONSTANT)
   QStringList txTypesList() const { return txTypes_; }

   Q_PROPERTY(QString generatedAddress READ generatedAddress NOTIFY addressGenerated)
   QString generatedAddress() const { return QString::fromStdString(generatedAddress_.display()); }

   // Settings properties
   Q_PROPERTY(QString settingLogFile READ settingLogFile WRITE setLogFile NOTIFY settingChanged)
   QString settingLogFile() { return getSettingStringAt(ApplicationSettings::Setting::logDefault, 0); }
   void setLogFile(const QString& str) { setSetting(ApplicationSettings::Setting::logDefault, str); }

   Q_PROPERTY(QString settingMsgLogFile READ settingMsgLogFile WRITE setMsgLogFile NOTIFY settingChanged)
   QString settingMsgLogFile() { return getSettingStringAt(ApplicationSettings::Setting::logMessages, 0); }
   void setMsgLogFile(const QString& str) { setSetting(ApplicationSettings::Setting::logMessages, str); }

   Q_PROPERTY(bool settingAdvancedTX READ settingAdvancedTX WRITE setAdvancedTX NOTIFY settingChanged)
   bool settingAdvancedTX() { return getSetting(ApplicationSettings::Setting::AdvancedTxDialogByDefault).toBool(); }
   void setAdvancedTX(bool b) { setSetting(ApplicationSettings::Setting::AdvancedTxDialogByDefault, b); }

   Q_PROPERTY(int settingEnvironment READ settingEnvironment WRITE setEnvironment NOTIFY settingChanged)
   int settingEnvironment() { return getSetting(ApplicationSettings::Setting::envConfiguration).toInt(); }
   void setEnvironment(int i) { setSetting(ApplicationSettings::Setting::envConfiguration, i); }

   Q_PROPERTY(QStringList settingEnvironments READ settingEnvironments)
   QStringList settingEnvironments() const;

   Q_PROPERTY(bool settingActivated READ settingActivated WRITE setActivated NOTIFY settingChanged)
   bool settingActivated() const { return getSetting(ApplicationSettings::Setting::initialized).toBool(); }
   void setActivated(bool b) { setSetting(ApplicationSettings::Setting::initialized, b); }
   
   Q_PROPERTY(QString settingExportDir READ settingExportDir WRITE setExportDir NOTIFY settingChanged)
   QString settingExportDir() const { return getSetting(ApplicationSettings::Setting::ExportDir).toString(); }
   void setExportDir(const QString& str) {setSetting(ApplicationSettings::Setting::ExportDir, str); }

   Q_PROPERTY(int armoryState READ armoryState NOTIFY armoryStateChanged)
   int armoryState() const { return armoryState_; }

   Q_PROPERTY(HwDeviceModel* devices READ devices NOTIFY devicesChanged)
   HwDeviceModel* devices();
   Q_PROPERTY(bool scanningDevices READ scanningDevices NOTIFY scanningChanged)
   bool scanningDevices() const;

   Q_PROPERTY(qtquick_gui::WalletPropertiesVM* walletProperitesVM READ walletProperitesVM CONSTANT)
   qtquick_gui::WalletPropertiesVM* walletProperitesVM() const;

   // QML-invokable methods
   Q_INVOKABLE QStringList newSeedPhrase();
   Q_INVOKABLE QStringList completeBIP39dic(const QString& prefix);
   Q_INVOKABLE void copySeedToClipboard(const QStringList&);
   Q_INVOKABLE void createWallet(const QString& name, const QStringList& seed
      , const QString& password);
   Q_INVOKABLE void importWallet(const QString& name, const QStringList& seed
      , const QString& password);
   Q_INVOKABLE void pollHWWallets();
   Q_INVOKABLE void stopHWWalletsPolling();
   Q_INVOKABLE void setHWpin(const QString&);
   Q_INVOKABLE void setHWpassword(const QString&);
   Q_INVOKABLE void importHWWallet(int deviceIndex);
   Q_INVOKABLE void generateNewAddress(int walletIndex, bool isNative);
   Q_INVOKABLE void copyAddressToClipboard(const QString& addr);
   Q_INVOKABLE QString pasteTextFromClipboard();
   Q_INVOKABLE bool validateAddress(const QString& addr);
   Q_INVOKABLE ArmoryServersModel* getArmoryServers();
   Q_INVOKABLE bool addArmoryServer(ArmoryServersModel*, const QString& name
      , int netType, const QString& ipAddr, const QString& ipPort, const QString& key = {});
   Q_INVOKABLE bool delArmoryServer(ArmoryServersModel*, int idx);

   Q_INVOKABLE void requestFeeSuggestions();
   Q_INVOKABLE QTXSignRequest* createTXSignRequest(int walletIndex, const QStringList& recvAddrs
      , const QList<double>& recvAmounts, double fee, const QString& comment = {}, bool isRbf = true, QUTXOList* utxos = nullptr);
   Q_INVOKABLE void getUTXOsForWallet(int walletIndex);
   Q_INVOKABLE void signAndBroadcast(QTXSignRequest*, const QString& password);
   Q_INVOKABLE int getSearchInputType(const QString&);
   Q_INVOKABLE void startAddressSearch(const QString&);
   Q_INVOKABLE QTxDetails* getTXDetails(const QString& txHash);
   Q_INVOKABLE int changePassword(const QString& walletId, const QString& oldPassword, const QString& newPassword);
   Q_INVOKABLE int exportWallet(const QString& walletId, const QString & exportDir);
   Q_INVOKABLE int viewWalletSeedAuth(const QString& walletId, const QString& password);
   Q_INVOKABLE int deleteWallet(const QString& walletId, const QString& password);
   Q_INVOKABLE int rescanWallet(const QString& walletId);
   Q_INVOKABLE void walletSelected(int);
   void notifyNewTransaction(const bs::TXEntry& tx);

signals:
   void walletsListChanged();
   void walletBalanceChanged();
   void addressGenerated();
   void settingChanged();
   void armoryStateChanged();
   void devicesChanged();
   void scanningChanged();
   void invokePINentry();
   void invokePasswordEntry(const QString& devName, bool acceptOnDevice);
   void showError(const QString&);
   void showNotification(QString, QString);
   void successExport(QString nameExport);

private slots:
   void onArmoryServerChanged(const QModelIndex&, const QVariant&);

private:
   bs::message::ProcessingResult processSettings(const bs::message::Envelope &);
   bs::message::ProcessingResult processSettingsGetResponse(const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   bs::message::ProcessingResult processSettingsState(const BlockSettle::Terminal::SettingsMessage_SettingsResponse&);
   bs::message::ProcessingResult processArmoryServers(bs::message::SeqId
      , const BlockSettle::Terminal::SettingsMessage_ArmoryServers&);
   bs::message::ProcessingResult processAdminMessage(const bs::message::Envelope &);
   bs::message::ProcessingResult processBlockchain(const bs::message::Envelope &);
   bs::message::ProcessingResult processSigner(const bs::message::Envelope &);
   bs::message::ProcessingResult processWallets(const bs::message::Envelope &);
   bs::message::ProcessingResult processHWW(const bs::message::Envelope&);

   void requestInitialSettings();
   void updateSplashProgress();
   void splashProgressCompleted();
   void updateStates();
   void setTopBlock(uint32_t);

   void createWallet(bool primary);
   std::string hdWalletIdByIndex(int);
   std::string generateWalletName() const;
   std::string hdWalletIdByLeafId(const std::string&) const;

   void processWalletLoaded(const bs::sync::WalletInfo &);
   bs::message::ProcessingResult processWalletData(const bs::message::SeqId
      , const BlockSettle::Common::WalletsMessage_WalletData&);
   bs::message::ProcessingResult processWalletBalances(bs::message::SeqId, const BlockSettle::Common::WalletsMessage_WalletBalances &);
   bs::message::ProcessingResult processTXDetails(bs::message::SeqId, const BlockSettle::Common::WalletsMessage_TXDetailsResponse &);
   bs::message::ProcessingResult processLedgerEntries(const BlockSettle::Common::LedgerEntries &);
   bs::message::ProcessingResult processAddressHist(const BlockSettle::Common::ArmoryMessage_AddressHistory&);
   bs::message::ProcessingResult processFeeLevels(const BlockSettle::Common::ArmoryMessage_FeeLevelsResponse&);
   bs::message::ProcessingResult processWalletsList(const BlockSettle::Common::WalletsMessage_WalletsListResponse&);
   bs::message::ProcessingResult processWalletDeleted(const std::string& walletId);
   bs::message::ProcessingResult processWalletSeed(const BlockSettle::Common::SignerMessage_WalletSeed&);
   bs::message::ProcessingResult processUTXOs(const BlockSettle::Common::WalletsMessage_UtxoListResponse&);
   bs::message::ProcessingResult processSignTX(const BlockSettle::Common::SignerMessage_SignTxResponse&);
   bs::message::ProcessingResult processZC(const BlockSettle::Common::ArmoryMessage_ZCReceived&);
   bs::message::ProcessingResult processZCInvalidated(const BlockSettle::Common::ArmoryMessage_ZCInvalidated&);
   bs::message::ProcessingResult processTransactions(bs::message::SeqId, const BlockSettle::Common::ArmoryMessage_Transactions&);
   bs::message::ProcessingResult processReservedUTXOs(const BlockSettle::Common::WalletsMessage_ReservedUTXOs&);
   void processWalletAddresses(const std::string& walletId, const std::vector<bs::sync::Address>&);
   bs::message::ProcessingResult processTxResponse(bs::message::SeqId
      , const BlockSettle::Common::WalletsMessage_TxResponse&);
   bs::message::ProcessingResult processUTXOs(bs::message::SeqId
      , const BlockSettle::Common::ArmoryMessage_UTXOs&);

   bs::message::ProcessingResult processHWDevices(const BlockSettle::HW::DeviceMgrMessage_Devices&);
   bs::message::ProcessingResult processHWWready(const std::string& walletId);
   bs::message::ProcessingResult processHWSignedTX(const BlockSettle::HW::DeviceMgrMessage_SignTxResponse&);

   QVariant getSetting(ApplicationSettings::Setting) const;
   QString getSettingStringAt(ApplicationSettings::Setting, int idx);
   void setSetting(ApplicationSettings::Setting, const QVariant&);

private:
   std::shared_ptr<spdlog::logger>        logger_;
   BSTerminalSplashScreen* splashScreen_{ nullptr };
   QObject* rootObj_{ nullptr };
   QQmlContext* rootCtxt_{nullptr};
   std::shared_ptr<bs::message::UserTerminal>   userSettings_, userWallets_;
   std::shared_ptr<bs::message::UserTerminal>   userBlockchain_, userSigner_;
   std::shared_ptr<bs::message::UserTerminal>   userHWW_;
   bool loadingDone_{ false };

   std::recursive_mutex mutex_;
   std::set<int>  createdComponents_;
   std::set<int>  loadingComponents_;
   int         armoryState_{ -1 };
   uint32_t    blockNum_{ 0 };
   int         signerState_{ -1 };
   std::string signerDetails_;
   bool  walletsReady_{ false };

   std::unordered_map<std::string, bs::sync::WalletInfo> hdWallets_;
   std::unordered_map<std::string, std::string> walletNames_;
   std::map<bs::message::SeqId, std::string> walletInfoReq_;
   std::map<bs::Address, std::string>  addrComments_;

   const QStringList txTypes_;
   QmlAddressListModel* addrModel_{ nullptr };
   TxListModel* txModel_{ nullptr };
   TxListForAddr* expTxByAddrModel_{ nullptr };
   TxInputsModel* txInputsModel_{ nullptr };
   TxInputsSelectedModel * txInputsSelectedModel_{ nullptr };
   TxOutputsModel* txOutputsModel_{ nullptr };
   HwDeviceModel* hwDeviceModel_{ nullptr };
   WalletBalancesModel* walletBalances_{ nullptr };
   FeeSuggestionModel* feeSuggModel_{ nullptr };
   std::unique_ptr<qtquick_gui::WalletPropertiesVM> walletPropertiesModel_;
   bs::Address generatedAddress_;
   bool hwDevicesPolling_{ false };
   bs::hww::DeviceKey curAuthDevice_{};

   struct TXReq {
      QTXSignRequest* txReq;
      bool  isMaxAmount{ false };
      BlockSettle::Common::WalletsMessage msg{};
   };
   std::map<bs::message::SeqId, TXReq>    txReqs_;
   std::unordered_map<std::string, QTXSignRequest*>   hwwReady_;
   std::map<bs::message::SeqId, QTxDetails*>          txDetailReqs_;
   std::map<ApplicationSettings::Setting, QVariant>   settingsCache_;
   std::set<bs::message::SeqId>  expTxAddrReqs_, expTxAddrInReqs_;
   std::map<bs::Address, std::string>  addressCache_;
   std::set<BinaryData> rmTxOnInvalidation_;
   std::map<bs::message::SeqId, ArmoryServersModel*>  armoryServersReq_;
};

#endif	// QT_QUICK_ADAPTER_H
