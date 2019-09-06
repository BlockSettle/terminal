#include "ChatProtocol/ClientDatabaseCreator.h"

using namespace Chat;

namespace {
   namespace party {
      const QString kTableName = QLatin1String("party");
      const QString kPartyTableId = QLatin1String("id");
      const QString kPartyId = QLatin1String("party_id");
      const QString kPartyDisplayName = QLatin1String("party_display_name");
   }

   namespace partyMessage {
      const QString kTableName = QLatin1String("party_message");
      const QString kPartyMessageTableId = QLatin1String("id");
      const QString kPartyTableId = QLatin1String("party_table_id");
      const QString kMessageId = QLatin1String("message_id");
      const QString kTimestamp = QLatin1String("timestamp");
      const QString kMessageState = QLatin1String("message_state");
      const QString kEncryptionType = QLatin1String("encryption_type");
      const QString kNonce = QLatin1String("nonce");
      const QString kMessageText = QLatin1String("message_text");
      const QString kSender = QLatin1String("sender");
   }
}

const static QMap <QString, TableStructure> clientTablesMap{
   {party::kTableName,
      {
         { //Table columns
            {party::kPartyTableId, QLatin1String("INTEGER PRIMARY KEY AUTOINCREMENT")},
            {party::kPartyId, QLatin1String("TEXT NOT NULL")},
            {party::kPartyDisplayName, QLatin1String("TEXT NOT NULL")}
         }
      }
   },
   {partyMessage::kTableName,
      {
         { //Table columns
            {partyMessage::kPartyMessageTableId, QLatin1String("INTEGER PRIMARY KEY AUTOINCREMENT")},
            {partyMessage::kPartyTableId, QLatin1String("INTEGER NOT NULL")},
            {partyMessage::kMessageId, QLatin1String("CHAR(32) NOT NULL")},
            {partyMessage::kTimestamp, QLatin1String("INTEGER NOT NULL")},
            {partyMessage::kMessageState, QLatin1String("INTEGER NOT NULL")},
            {partyMessage::kEncryptionType, QLatin1String("INTEGER NOT NULL")},
            {partyMessage::kNonce, QLatin1String("BLOB")},
            {partyMessage::kMessageText, QLatin1String("TEXT")},
            {partyMessage::kSender, QLatin1String("TEXT")}
         },
         { //Foreign keys
            {partyMessage::kPartyTableId, party::kTableName, party::kPartyTableId}
         }
      }
   }
};

ClientDatabaseCreator::ClientDatabaseCreator(const QSqlDatabase& db, const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */)
   : DatabaseCreator(db, loggerPtr, parent)
{
   tablesMap_ = clientTablesMap;
}

void ClientDatabaseCreator::rebuildDatabase()
{
   requiredTables_ = QStringList(
      {
         party::kTableName,
         partyMessage::kTableName
      }
   );

   DatabaseCreator::rebuildDatabase();
}
