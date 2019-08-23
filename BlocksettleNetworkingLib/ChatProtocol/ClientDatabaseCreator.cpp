#include "ChatProtocol/ClientDatabaseCreator.h"

namespace Chat
{

   namespace {
      namespace party {
         const QString TABLE_NAME = QLatin1String("party");
         const QString PARTY_TABLE_ID = QLatin1String("id");
         const QString PARTY_ID = QLatin1String("party_id");
         const QString PARTY_DISPLAY_NAME = QLatin1String("party_display_name");
      }

      namespace partyMessage {
         const QString TABLE_NAME = QLatin1String("party_message");
         const QString PARTY_MESSAGE_TABLE_ID = QLatin1String("id");
         const QString PARTY_TABLE_ID = QLatin1String("party_table_id");
         const QString MESSAGE_ID = QLatin1String("message_id");
         const QString TIMESTAMP = QLatin1String("timestamp");
         const QString MESSAGE_STATE = QLatin1String("message_state");
         const QString ENCRYPTION_TYPE = QLatin1String("encryption_type");
         const QString NONCE = QLatin1String("nonce");
         const QString MESSAGE_TEXT = QLatin1String("message_text");
         const QString SENDER = QLatin1String("sender");
      }
   }

   const static QMap <QString, TableStructure> clientTablesMap {
      {party::TABLE_NAME,
         {
            { //Table columns
               {party::PARTY_TABLE_ID, QLatin1String("INTEGER PRIMARY KEY AUTOINCREMENT")},
               {party::PARTY_ID, QLatin1String("TEXT NOT NULL")},
               {party::PARTY_DISPLAY_NAME, QLatin1String("TEXT NOT NULL")}
            }
         }
      },
      {partyMessage::TABLE_NAME,
         {
            { //Table columns
               {partyMessage::PARTY_MESSAGE_TABLE_ID, QLatin1String("INTEGER PRIMARY KEY AUTOINCREMENT")},
               {partyMessage::PARTY_TABLE_ID, QLatin1String("INTEGER NOT NULL")},
               {partyMessage::MESSAGE_ID, QLatin1String("CHAR(32) NOT NULL")},
               {partyMessage::TIMESTAMP, QLatin1String("INTEGER NOT NULL")},
               {partyMessage::MESSAGE_STATE, QLatin1String("INTEGER NOT NULL")},
               {partyMessage::ENCRYPTION_TYPE, QLatin1String("INTEGER NOT NULL")},
               {partyMessage::NONCE, QLatin1String("BLOB")},
               {partyMessage::MESSAGE_TEXT, QLatin1String("TEXT")},
               {partyMessage::SENDER, QLatin1String("TEXT")}
            },
            { //Foreign keys
               {partyMessage::PARTY_TABLE_ID, party::TABLE_NAME, party::PARTY_TABLE_ID}
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
            party::TABLE_NAME,
            partyMessage::TABLE_NAME
         }
      );

      DatabaseCreator::rebuildDatabase();
   }
}