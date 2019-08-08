#ifndef ClientDatabaseCreator_h__
#define ClientDatabaseCreator_h__

#include <memory>

#include "ChatProtocol/DatabaseCreator.h"

namespace Chat
{

   class ClientDatabaseCreator : public DatabaseCreator
   {
      Q_OBJECT
   public:
      explicit ClientDatabaseCreator(const QSqlDatabase& db, const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      void rebuildDatabase() override;
   };

   using ClientDatabaseCreatorPtr = std::shared_ptr<ClientDatabaseCreator>;
}

#endif // ClientDatabaseCreator_h__
