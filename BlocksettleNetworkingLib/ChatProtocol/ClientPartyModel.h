#ifndef PartyModel_h__
#define PartyModel_h__

#include <QObject>
#include <memory>

namespace Chat
{

   class PartyModel : public QObject
   {
      Q_OBJECT
   public:
      PartyModel(QObject* parent = nullptr);
   };

}

#endif // PartyModel_h__
