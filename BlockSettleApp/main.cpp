/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QScreen>
#include <QStandardPaths>
#include <QtPlugin>
#include <memory>
#include "ApplicationSettings.h"
#include "BSErrorCode.h"
#include "BSMessageBox.h"
#include "BSTerminalSplashScreen.h"
#include "EncryptionUtils.h"

#include "Adapters/AuthEidAdapter.h"
#include "Adapters/BlockchainAdapter.h"
#include "Adapters/OnChainTrackerAdapter.h"
#include "Adapters/WalletsAdapter.h"
#include "ApiAdapter.h"
#include "ApiJson.h"
#include "AssetsAdapter.h"
#include "BsServerAdapter.h"
#include "ChatAdapter.h"
#include "MatchingAdapter.h"
#include "MDHistAdapter.h"
#include "MktDataAdapter.h"
#include "QtGuiAdapter.h"
#include "SettingsAdapter.h"
#include "SettlementAdapter.h"
#include "SignerAdapter.h"

#include "btc/ecc.h"
#include <spdlog/sinks/daily_file_sink.h>

//#include "AppNap.h"

#ifdef USE_QWindowsIntegrationPlugin
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
Q_IMPORT_PLUGIN(QWindowsPrinterSupportPlugin)
#endif // USE_QWindowsIntegrationPlugin

#ifdef USE_QCocoaIntegrationPlugin
Q_IMPORT_PLUGIN(QCocoaIntegrationPlugin)
Q_IMPORT_PLUGIN(QCocoaPrinterSupportPlugin)
#endif // USE_QCocoaIntegrationPlugin

#ifdef USE_QXcbIntegrationPlugin
Q_IMPORT_PLUGIN(QXcbIntegrationPlugin)
Q_IMPORT_PLUGIN(QCupsPrinterSupportPlugin)
#endif // USE_QXcbIntegrationPlugin

#ifdef STATIC_BUILD
Q_IMPORT_PLUGIN(QSQLiteDriverPlugin)
Q_IMPORT_PLUGIN(QICOPlugin)
#endif // STATIC_BUILD

Q_DECLARE_METATYPE(ArmorySettings)
Q_DECLARE_METATYPE(AsyncClient::LedgerDelegate)
Q_DECLARE_METATYPE(BinaryData)
Q_DECLARE_METATYPE(bs::error::AuthAddressSubmitResult);
Q_DECLARE_METATYPE(CelerAPI::CelerMessageType);
Q_DECLARE_METATYPE(SecureBinaryData)
Q_DECLARE_METATYPE(std::shared_ptr<std::promise<bool>>)
Q_DECLARE_METATYPE(std::string)
Q_DECLARE_METATYPE(std::vector<BinaryData>)
Q_DECLARE_METATYPE(std::vector<UTXO>)
Q_DECLARE_METATYPE(UTXO)

#include <QEvent>
#include <QApplicationStateChangeEvent>

class MacOsApp : public QApplication
{
   Q_OBJECT
public:
   MacOsApp(int &argc, char **argv) : QApplication(argc, argv) {}
   ~MacOsApp() override = default;

signals:
   void reactivateTerminal();

protected:
   bool event(QEvent* ev) override
   {
      if (ev->type() ==  QEvent::ApplicationStateChange) {
         auto appStateEvent = static_cast<QApplicationStateChangeEvent*>(ev);

         if (appStateEvent->applicationState() == Qt::ApplicationActive) {
            if (activationRequired_) {
               emit reactivateTerminal();
            } else {
               activationRequired_ = true;
            }
         } else {
            activationRequired_ = false;
         }
      }

      return QApplication::event(ev);
   }

private:
   bool activationRequired_ = false;
};

static void checkStyleSheet(QApplication &app)
{
   QLatin1String styleSheetFileName = QLatin1String("stylesheet.css");

   QFileInfo info = QFileInfo(QLatin1String(styleSheetFileName));

   static QDateTime lastTimestamp = info.lastModified();

   if (lastTimestamp == info.lastModified()) {
      return;
   }

   lastTimestamp = info.lastModified();

   QFile stylesheetFile(styleSheetFileName);

   bool result = stylesheetFile.open(QFile::ReadOnly);
   assert(result);

   app.setStyleSheet(QString::fromLatin1(stylesheetFile.readAll()));
}

static QScreen *getDisplay(QPoint position)
{
   for (auto currentScreen : QGuiApplication::screens()) {
      if (currentScreen->availableGeometry().contains(position, false)) {
         return currentScreen;
      }
   }

   return QGuiApplication::primaryScreen();
}

int main(int argc, char** argv)
{
   srand(std::time(nullptr));

   // Initialize libbtc, BIP 150, and BIP 151. 150 uses the proprietary "public"
   // Armory setting designed to allow the ArmoryDB server to not have to verify
   // clients. Prevents us from having to import tons of keys into the server.
   btc_ecc_start();
   startupBIP151CTX();
   startupBIP150CTX(4);

   QStringList args;
   for (int i = 0; i < argc; ++i) {
      args << QLatin1String(argv[i]);
   }
#ifdef NDEBUG
   try {
#endif   //NDEBUG
      const auto &settings = std::make_shared<ApplicationSettings>(QLatin1Literal("BlockSettle Terminal")
         , QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QDir::separator() + ApplicationSettings::appSubDir());
      const auto &adSettings = std::make_shared<SettingsAdapter>(settings, args);
      const auto &logMgr = adSettings->logManager();

      bs::message::TerminalInprocBus inprocBus(logMgr->logger());
      inprocBus.addAdapter(adSettings);

      const auto &apiAdapter = std::make_shared<ApiAdapter>(logMgr->logger("API"));
      const auto &guiAdapter = std::make_shared<QtGuiAdapter>(logMgr->logger("ui"));
      apiAdapter->add(guiAdapter);
      apiAdapter->add(std::make_shared<ApiJsonAdapter>(logMgr->logger("json")));
      inprocBus.addAdapter(apiAdapter);

      const auto &signAdapter = std::make_shared<SignerAdapter>(logMgr->logger());
      inprocBus.addAdapter(signAdapter);

      const auto& userBlockchain = bs::message::UserTerminal::create(bs::message::TerminalUsers::Blockchain);
      const auto& userWallets = bs::message::UserTerminal::create(bs::message::TerminalUsers::Wallets);
      inprocBus.addAdapter(std::make_shared<OnChainTrackerAdapter>(logMgr->logger("trk")
         , bs::message::UserTerminal::create(bs::message::TerminalUsers::OnChainTracker)
         , userBlockchain, userWallets, adSettings->createOnChainPlug()));
      inprocBus.addAdapter(std::make_shared<AssetsAdapter>(logMgr->logger()));
      inprocBus.addAdapter(std::make_shared<WalletsAdapter>(logMgr->logger()
         , userWallets, signAdapter->createClient(), userBlockchain));
      inprocBus.addAdapter(std::make_shared<BsServerAdapter>(logMgr->logger("bscon")));

      inprocBus.addAdapter(std::make_shared<MatchingAdapter>(logMgr->logger("match")));
      inprocBus.addAdapter(std::make_shared<SettlementAdapter>(logMgr->logger("settl")));
      inprocBus.addAdapter(std::make_shared<MktDataAdapter>(logMgr->logger("md")));
      inprocBus.addAdapter(std::make_shared<MDHistAdapter>(logMgr->logger("mdh")));
      inprocBus.addAdapter(std::make_shared<ChatAdapter>(logMgr->logger("chat")));
      inprocBus.addAdapter(std::make_shared<BlockchainAdapter>(logMgr->logger()
         , userBlockchain));

      if (!inprocBus.run(argc, argv)) {
         logMgr->logger()->error("No runnable adapter found on main inproc bus");
         return EXIT_FAILURE;
      }
#ifdef NDEBUG
   }
   catch (const std::exception &e) {
      std::cerr << "Failed to start BlockSettle Terminal: " << e.what() << std::endl;
      BSMessageBox(BSMessageBox::critical, QObject::tr("BlockSettle Terminal")
         , QObject::tr("Unhandled exception detected: %1").arg(QLatin1String(e.what()))).exec();
      return EXIT_FAILURE;
   }
#endif   //NDEBUG
}

#include "main.moc"
