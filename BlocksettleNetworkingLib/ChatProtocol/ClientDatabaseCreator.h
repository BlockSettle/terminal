#ifndef CLIENTDATABASECREATOR_H
#define CLIENTDATABASECREATOR_H

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

#endif // CLIENTDATABASECREATOR_H
