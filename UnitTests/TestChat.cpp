#include <gtest/gtest.h>
#include <QString>
#include "TestEnv.h"

/*
TEST(TestChat, PerfEncryptEC)
{
   const auto &cbGetMsgText = [](int index) -> QString {
      static const std::vector<std::string> texts = {
         "Short message text",
         "Slightly longer message text",
         "Long message text - long message text - long message text",
         "Very long message text - very long message text - very long message text - very long message text"
      };
      return QString::fromStdString(texts[index % texts.size()]);
   };
   const size_t nbIter = 1000;
   const QString sender = QLatin1String("abcdefg");
   const QString receiver = QLatin1String("gfedcba");
   const QString msgId = QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr());
   const auto privKey = autheid::generatePrivateKey();
   const auto pubKey = autheid::getPublicKey(privKey);

   const auto &signMessage = [privKey](const Chat::MessageData &msg) -> autheid::Bytes {
      QJsonObject data;
      data[QLatin1String("sender")] = msg.senderId();
      data[QLatin1String("receiver")] = msg.receiverId();
      data[QLatin1String("timestamp")] = msg.dateTime().toMSecsSinceEpoch();
      data[QLatin1String("text")] = msg.messageData();
      QJsonDocument doc(data);
      const auto jsonData = doc.toJson(QJsonDocument::Compact).toStdString();
      return autheid::signData(jsonData.data(), jsonData.length(), privKey);
   };

   for (size_t i = 0; i < nbIter; ++i) {
      const QDateTime timestamp = QDateTime::currentDateTimeUtc();
      Chat::MessageData msg(sender, receiver, msgId, timestamp, cbGetMsgText(i));
//      msg.encrypt(pubKey);  //FIXME!
      signMessage(msg);
   }
}

TEST(TestChat, PerfDecryptEC)
{
   const size_t nbIter = 1000;
   const QString sender = QLatin1String("abcdefg");
   const QString receiver = QLatin1String("gfedcba");
   const QString msgId = QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr());
   const QDateTime timestamp = QDateTime::currentDateTimeUtc();
   const QString msgText = QLatin1String("Message text used to test decryption speed");
   const auto privKey = autheid::generatePrivateKey();
   const auto pubKey = autheid::getPublicKey(privKey);
   Chat::MessageData msg(sender, receiver, msgId, timestamp, msgText);
//   msg.encrypt(pubKey);  //FIXME!

   for (size_t i = 0; i < nbIter; ++i) {
      auto msgCopy = msg;
//      msgCopy.decrypt(privKey);   //FIXME!
   }
}

TEST(TestChat, PerfVerifySigEC)
{
   const size_t nbIter = 1000;
   const QString sender = QLatin1String("abcdefg");
   const QString receiver = QLatin1String("gfedcba");
   const QString msgId = QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr());
   const QDateTime timestamp = QDateTime::currentDateTimeUtc();
   const QString msgText = QLatin1String("Message text used to test signature verification speed");
   const auto privKey = autheid::generatePrivateKey();
   const auto pubKey = autheid::getPublicKey(privKey);
   Chat::MessageData msg(sender, receiver, msgId, timestamp, msgText);
//   msg.encrypt(pubKey);  //FIXME!

   QJsonObject data;
   data[QLatin1String("sender")] = msg.senderId();
   data[QLatin1String("receiver")] = msg.receiverId();
   data[QLatin1String("timestamp")] = msg.dateTime().toMSecsSinceEpoch();
   data[QLatin1String("text")] = msg.messageData();
   QJsonDocument doc(data);
   const auto jsonData = doc.toJson(QJsonDocument::Compact).toStdString();
   const auto signature = autheid::signData(jsonData.data(), jsonData.length(), privKey);

   for (size_t i = 0; i < nbIter; ++i) {
      autheid::verifyData(jsonData.data(), jsonData.length(), signature.data(), signature.size(), pubKey);
   }
}
*/