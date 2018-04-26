#include <gtest/gtest.h>

#include <QApplication>
#include <QDebug>
#include <QLocale>
#include <QString>

#include "CommonTypes.h"
#include "CustomDoubleSpinBox.h"
#include "CustomDoubleValidator.h"
#include "HDWallet.h"
#include "RequestingQuoteWidget.h"
#include "RFQTicketXBT.h"
#include "TestEnv.h"
#include "UiUtils.h"
#include "WalletsManager.h"

TEST(TestUi, ValidateString)
{
   int pos = 0;

   ASSERT_EQ(8, UiUtils::GetAmountPrecisionXBT());
   ASSERT_EQ(2, UiUtils::GetAmountPrecisionFX());
   ASSERT_EQ(0, UiUtils::GetAmountPrecisionCC());

   const int decimalsXBT = UiUtils::GetAmountPrecisionXBT();
   const int decimalsFX  = UiUtils::GetAmountPrecisionFX();
   const int decimalsCC  = UiUtils::GetAmountPrecisionCC();

   QString correctXbt = QString::fromStdString("1 123.12345678");
   EXPECT_EQ(QValidator::Acceptable, UiUtils::ValidateDoubleString(correctXbt, pos, decimalsXBT));

   QString correctDecilamlsOnlyXBT = QString::fromStdString(".12345678");
   EXPECT_EQ(QValidator::Acceptable, UiUtils::ValidateDoubleString(correctDecilamlsOnlyXBT, pos, decimalsXBT));

   QString invalidLengthXBT = QString::fromStdString("1123.123456789");
   EXPECT_EQ(QValidator::Invalid, UiUtils::ValidateDoubleString(invalidLengthXBT, pos, decimalsXBT));

   QString invalidSymbolsXBT = QString::fromStdString("1123,123456");
   EXPECT_EQ(QValidator::Invalid, UiUtils::ValidateDoubleString(invalidSymbolsXBT, pos, decimalsXBT));

   QString negativeXBT = QString::fromStdString("-1123.123456");
   EXPECT_EQ(QValidator::Invalid, UiUtils::ValidateDoubleString(negativeXBT, pos, decimalsXBT));

   // CC
   QString corerctCC = QString::fromStdString("1 123 456");
   EXPECT_EQ(QValidator::Acceptable, UiUtils::ValidateDoubleString(corerctCC, pos, decimalsCC));

   QString invalidSymbolsCC = QString::fromStdString("1123.");
   EXPECT_EQ(QValidator::Invalid, UiUtils::ValidateDoubleString(invalidSymbolsCC, pos, decimalsCC));

   QString invalidSymbolsCC2 = QString::fromStdString(".");
   EXPECT_EQ(QValidator::Invalid, UiUtils::ValidateDoubleString(invalidSymbolsCC2, pos, decimalsCC));

   QString invalidSymbolsCC3 = QString::fromStdString("1,123,123");
   EXPECT_EQ(QValidator::Invalid, UiUtils::ValidateDoubleString(invalidSymbolsCC3, pos, decimalsCC));

   QString negativeCC = QString::fromStdString("-1123");
   EXPECT_EQ(QValidator::Invalid, UiUtils::ValidateDoubleString(negativeCC, pos, decimalsCC));
}

TEST(TestUi, Display)
{
   EXPECT_EQ(UiUtils::displayAmount(12.34567), QLocale().toString(12.34567, 'f', 8));
   EXPECT_EQ(UiUtils::displayCurrencyAmount(12.345), QLocale().toString(12.35, 'f', 2));
   EXPECT_EQ(UiUtils::displayCCAmount(12.345), QLatin1String("12"));
   EXPECT_EQ(UiUtils::amountToBtc(12345678), 0.12345678);
   EXPECT_EQ(UiUtils::displayQuantity(12.34567, bs::network::XbtCurrency), QObject::tr("XBT %1").arg(QLocale().toString(12.34567000, 'f', 8)));
   EXPECT_EQ(UiUtils::displayQuantity(12.34567, std::string("USD")), QObject::tr("USD %1").arg(QLocale().toString(12.35, 'f', 2)));

   EXPECT_EQ(UiUtils::displayQty(12.345, "EUR/USD", "EUR", bs::network::Asset::SpotFX), QLocale().toString(12.35, 'f', 2));
   EXPECT_EQ(UiUtils::displayQty(12.345, "XBT/USD", "XBT", bs::network::Asset::SpotXBT), QLocale().toString(12.345, 'f', 8));
   EXPECT_EQ(UiUtils::displayQty(12.345, "XBT/USD", "USD", bs::network::Asset::SpotXBT), QLocale().toString(12.35, 'f', 2));
   EXPECT_EQ(UiUtils::displayQty(12.01, "BLK/XBT", "BLK", bs::network::Asset::PrivateMarket), QLocale().toString(12));

   EXPECT_EQ(UiUtils::displayValue(12.345, "EUR/USD", "EUR", bs::network::Asset::SpotFX), QLocale().toString(12.35, 'f', 2));
   EXPECT_EQ(UiUtils::displayValue(12.345, "XBT/USD", "XBT", bs::network::Asset::SpotXBT), QLocale().toString(12.35, 'f', 2));
   EXPECT_EQ(UiUtils::displayValue(12.345, "XBT/USD", "USD", bs::network::Asset::SpotXBT), QLocale().toString(12.345, 'f', 8));
   EXPECT_EQ(UiUtils::displayValue(12.01, "BLK/XBT", "BLK", bs::network::Asset::PrivateMarket), QLocale().toString(12.01, 'f', 6));
}

TEST(TestUi, RFQ_entry_CC_sell)
{
   TestEnv::walletsMgr()->CreateWallet("Primary", "", {}, true);
   ASSERT_TRUE(TestEnv::walletsMgr()->HasPrimaryWallet());
   const auto priWallet = TestEnv::walletsMgr()->GetPrimaryWallet();
   const auto ccGroup = priWallet->createGroup(bs::hd::CoinType::BlockSettle_CC);
   ASSERT_NE(ccGroup, nullptr);
   const auto ccLeaf = ccGroup->createLeaf("BLK");
   ASSERT_NE(ccLeaf, nullptr);
   const auto addr = ccLeaf->GetNewExtAddress();

   RFQTicketXBT ticket;
   ticket.init(TestEnv::authAddrMgr(), TestEnv::assetMgr(), TestEnv::quoteProvider(), nullptr);
   ticket.setWalletsManager(TestEnv::walletsMgr());
   ticket.resetTicket();
   emit TestEnv::assetMgr()->securitiesReceived();
   ticket.setSecuritySell(QLatin1String(bs::network::Asset::toString(bs::network::Asset::PrivateMarket)), QLatin1String("BLK/XBT")
      , QLatin1String("1.23"), QLatin1String("2.34"));

   const auto txData = ticket.GetTransactionData();
   EXPECT_NE(txData, nullptr);
   EXPECT_NE(txData->GetSigningWallet(), nullptr);
   EXPECT_EQ(txData->GetSigningWallet(), ccLeaf) << "Should be " << ccLeaf->GetWalletName() << " instead of "
      << txData->GetSigningWallet()->GetWalletName();

   bs::network::RFQ rfq;
   rfq.requestId = qrand();
   rfq.security = "BLK/XBT";
   rfq.product = "BLK";
   rfq.assetType = bs::network::Asset::PrivateMarket;
   rfq.side = bs::network::Side::Sell;
   rfq.quantity = 100;

   RequestingQuoteWidget rqw;
   rqw.SetAssetManager(TestEnv::assetMgr());
   rqw.populateDetails(rfq, nullptr);

   bs::network::Quote quote;
   quote.price = 0.0023;
   quote.quantity = 100;
   quote.quoteId = qrand();
   quote.requestId = rfq.requestId;
   quote.security = rfq.security;
   quote.product = rfq.product;
   quote.assetType = rfq.assetType;
   quote.side = bs::network::Side::invert(rfq.side);
   quote.timeSkewMs = 0;
   quote.expirationTime = QDateTime::currentDateTime().addSecs(30);
   quote.quotingType = bs::network::Quote::Indicative;
   EXPECT_TRUE(rqw.onQuoteReceived(quote));
   quote.quotingType = bs::network::Quote::Tradeable;
   EXPECT_TRUE(rqw.onQuoteReceived(quote));

   TestEnv::walletsMgr()->DeleteWalletFile(priWallet);
}
