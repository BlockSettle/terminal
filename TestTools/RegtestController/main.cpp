#ifdef _MSC_VER
#  include <winsock2.h>
#endif
#include <QCommandLineParser>
#include <QCoreApplication>
#include "BS_regtest.h"
#include "RegtestController.h"

static const std::string version = "0.0.1";

static const QString cmdBalance = QLatin1String("balance");
static const QString cmdDecodeTx = QLatin1String("decode_tx");
static const QString cmdGenBlocks = QLatin1String("generate_blocks");
static const QString cmdSpend = QLatin1String("spend");

int processCommands(const QStringList& args, const std::shared_ptr<RegtestController> &ctrl)
{
   QCommandLineParser parser;
   parser.setApplicationDescription(QLatin1String("Command-line tool for controlling BlockSettle's regtest instance"));
   parser.addHelpOption();

   parser.addOption({ cmdBalance, QLatin1String("Shows maximum balance available for spending") });
   parser.addOption({ cmdDecodeTx, QLatin1String("Decodes raw transaction presented as hex_tx"), QLatin1String("hex_tx") });
   parser.addOption({ cmdGenBlocks, QLatin1String("Generate numBlocks blocks"), QLatin1String("numBlocks") });
   parser.addOption({ cmdSpend, QLatin1String("Sends amount of regtest bitcoins to address"), QLatin1String("address:amount") });

   if (args.size() == 1) {
      parser.showHelp();
      return 0;
   }
   parser.process(args);

   if (parser.isSet(cmdBalance)) {
      std::cout << "Current regtest maximum balance: " << ctrl->GetBalance() << std::endl;
   }

   if (parser.isSet(cmdDecodeTx)) {
      const auto hexTx = parser.value(cmdDecodeTx);
      std::cout << "Raw transaction decoding result:" << std::endl;
      std::cout << ctrl->Decode(hexTx).toStdString() << std::endl;
   }

   if (parser.isSet(cmdGenBlocks)) {
      const unsigned int nBlocks = parser.value(cmdGenBlocks).toUInt();
      if (nBlocks < 1) {
         std::cerr << "Invalid blocks number: " << nBlocks << std::endl;
         return 1;
      }
      std::cout << "Generating " << nBlocks << " blocks..." << std::endl;
      if (!ctrl->GenerateBlocks(nBlocks)) {
         std::cerr << "Failed to generate blocks" << std::endl;
      }
   }

   if (parser.isSet(cmdSpend)) {
      const auto &arg = parser.value(cmdSpend);
      const auto posColon = arg.indexOf(QLatin1Char(':'));
      if (posColon < 20) {
         std::cerr << "Invalid spend argument" << std::endl;
         return 1;
      }
      bs::Address addr;
      try {
         addr = bs::Address(arg.left(posColon));
      }
      catch (const std::exception &e) {
         std::cerr << "Failed to parse address: " << e.what() << std::endl;
         return 1;
      }
      const auto amount = arg.mid(posColon + 1).toDouble();
      if (amount <= 0) {
         std::cerr << "Invalid amount: " << amount << std::endl;
         return 1;
      }
      const auto result = ctrl->SendTo(amount, addr);
      if (result.isEmpty() || result.isNull()) {
         std::cerr << "Failed to send " << amount << " to " << addr.display<std::string>() << std::endl;
      }
      else {
         std::cout << "Result of sending " << amount << " to " << addr.display<std::string>()
            << ": " << result.toStdString() << std::endl;
      }
   }
   return 0;
}

int main(int argc, char** argv)
{
#ifdef _MSC_VER
   WSADATA wsaData;
   WORD wVersion = MAKEWORD(2, 0);
   WSAStartup(wVersion, &wsaData);
#endif

   QCoreApplication app(argc, argv);
   app.setApplicationName(QLatin1String("regtestctrl"));
   app.setApplicationVersion(QString::fromStdString(version));
   std::cout << "BlockSettle regtest control v" << version << std::endl;

   BlockDataManagerConfig config;
   config.selectNetwork("Regtest");

   const auto ctrl = std::make_shared<RegtestController>(BS_REGTEST_HOST, BS_REGTEST_PORT, BS_REGTEST_AUTH_COOKIE);
   return processCommands(app.arguments(), ctrl);
}
