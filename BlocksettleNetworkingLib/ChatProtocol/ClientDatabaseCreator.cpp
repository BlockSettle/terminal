#include "ChatProtocol/ClientDatabaseCreator.h"

using namespace Chat;

namespace {
   namespace party {
      const QString kTableName = QStringLiteral("party");
      const QString kPartyTableId = QStringLiteral("id");
      const QString kPartyId = QStringLiteral("party_id");
      const QString kPartyDisplayName = QStringLiteral("party_display_name");
      const QString kPartyType = QStringLiteral("party_type");
      const QString kPartySubType = QStringLiteral("party_sub_type");
   }

   namespace partyMessage {
      const QString kTableName = QStringLiteral("party_message");
      const QString kPartyMessageTableId = QStringLiteral("id");
      const QString kPartyTableId = QStringLiteral("party_table_id");
      const QString kMessageId = QStringLiteral("message_id");
      const QString kTimestamp = QStringLiteral("timestamp");
      const QString kMessageState = QStringLiteral("message_state");
      const QString kEncryptionType = QStringLiteral("encryption_type");
      const QString kNonce = QStringLiteral("nonce");
      const QString kMessageText = QStringLiteral("message_text");
      const QString kSender = QStringLiteral("sender");
   }

   namespace user {
      const QString kTableName = QStringLiteral("user");
      const QString kUserTableId = QStringLiteral("user_id");
      const QString kUserHash = QStringLiteral("user_hash");
   }

   namespace userKey {
      const QString kTableName = QStringLiteral("user_key");
      const QString kUserTableId = QStringLiteral("user_table_id");
      const QString kPublicKey = QStringLiteral("public_key");
      const QString kPublicKeyTimestamp = QStringLiteral("public_key_timestamp");
   }

   namespace partyToUser {
      const QString kTableName = QStringLiteral("party_to_user");
      const QString kPartyToUserTableId = QStringLiteral("id");
      const QString kPartyTableId = QStringLiteral("party_table_id");
      const QString kUserTableId = QStringLiteral("user_table_id");
   }
}

const static QMap <QString, TableStructure> clientTablesMap{
   {party::kTableName,
      {
         { //Table columns
            {party::kPartyTableId, QStringLiteral("INTEGER PRIMARY KEY AUTOINCREMENT")},
            {party::kPartyId, QStringLiteral("TEXT NOT NULL UNIQUE")},
            {party::kPartyDisplayName, QStringLiteral("TEXT NOT NULL")},
            {party::kPartyType, QStringLiteral("INTEGER NOT NULL")},
            {party::kPartySubType, QStringLiteral("INTEGER NOT NULL")}
         }
      }
   },
   {partyMessage::kTableName,
      {
         { //Table columns
            {partyMessage::kPartyMessageTableId, QStringLiteral("INTEGER PRIMARY KEY AUTOINCREMENT")},
            {partyMessage::kPartyTableId, QStringLiteral("INTEGER NOT NULL")},
            {partyMessage::kMessageId, QStringLiteral("CHAR(36) NOT NULL UNIQUE")},
            {partyMessage::kTimestamp, QStringLiteral("INTEGER NOT NULL")},
            {partyMessage::kMessageState, QStringLiteral("INTEGER NOT NULL")},
            {partyMessage::kEncryptionType, QStringLiteral("INTEGER NOT NULL")},
            {partyMessage::kNonce, QStringLiteral("BLOB")},
            {partyMessage::kMessageText, QStringLiteral("TEXT")},
            {partyMessage::kSender, QStringLiteral("TEXT")}
         },
         { //Foreign keys
            {partyMessage::kPartyTableId, party::kTableName, party::kPartyTableId, {}}
         }
      }
   },
   {user::kTableName,
      {
         { //Table columns
            {user::kUserTableId, QStringLiteral("INTEGER PRIMARY KEY AUTOINCREMENT")},
            {user::kUserHash, QStringLiteral("TEXT NOT NULL UNIQUE")}
         }
      }
   },
   {userKey::kTableName,
      {
         { //Table columns
            {userKey::kUserTableId, QStringLiteral("INTEGER NOT NULL UNIQUE")},
            {userKey::kPublicKey, QStringLiteral("TEXT NOT NULL")},
            {userKey::kPublicKeyTimestamp, QStringLiteral("INTEGER NOT NULL")}         
         },
         { //Foreign key
            {userKey::kUserTableId, user::kTableName, user::kUserTableId, {}}
         }
      }
   },
   {partyToUser::kTableName,
      {
         { //Table columns
            {partyToUser::kPartyToUserTableId, QStringLiteral("INTEGER PRIMARY KEY AUTOINCREMENT")},
            {partyToUser::kPartyTableId, QStringLiteral("INTEGER NOT NULL")},
            {partyToUser::kUserTableId, QStringLiteral("INTEGER NOT NULL")},
         },
         { //Foreign keys
            {partyToUser::kPartyTableId, party::kTableName, party::kPartyTableId, {}},
            {partyToUser::kUserTableId, user::kTableName, user::kUserTableId, {}}
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
         partyMessage::kTableName,
         user::kTableName,
         partyToUser::kTableName,
         userKey::kTableName
      }
   );

   DatabaseCreator::rebuildDatabase();
}
