#ifndef RFQ_STORAGE_H
#define RFQ_STORAGE_H

#include <memory>
#include <unordered_map>
#include <QObject>

#include "ValidityFlag.h"

namespace bs {
   class SettlementContainer;
}

// Use to store and release memory for bs::SettlementContainer
class RfqStorage : public QObject
{
   Q_OBJECT

public:
   RfqStorage();
   ~RfqStorage();

   void addSettlementContainer(std::shared_ptr<bs::SettlementContainer> rfq);

   bs::SettlementContainer *settlementContainer(const std::string &id) const;

private:
   std::unordered_map<std::string, std::shared_ptr<bs::SettlementContainer>> rfqs_;

   ValidityFlag validityFlag_;

};

#endif
