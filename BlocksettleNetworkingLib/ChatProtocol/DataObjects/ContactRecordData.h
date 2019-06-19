#ifndef CONTACTRECORDDATA_H
#define CONTACTRECORDDATA_H

#include "DataObject.h"
#include "../ProtocolDefinitions.h"
#include <QDateTime>

namespace Chat {

   class ContactRecordData : public DataObject
   {
   public:
      ContactRecordData(const QString& userId,
         const QString& contactId,
         const ContactStatus &status,
         const QString &publicKey,
         const QDateTime &publicKeyTime = QDateTime(),
         const QString& displayName = QString());

      QString getUserId() const;
      void setUserId(const QString& userId);

      QString getContactId() const;
      ContactStatus getContactStatus() const;
      void setContactStatus(const ContactStatus& status);

      QString getContactPublicKey() const;
      BinaryData getContactPublicKeyBinaryData() const;

      QString getDisplayName() const;
      void setDisplayName(const QString& displayName);
      bool hasDisplayName() const;

      QDateTime getContactPublicKeyTime() const;
      void contactPublicKeyTime(const QDateTime& publicKeyTime);

      QJsonObject toJson() const override;
      static std::shared_ptr<ContactRecordData> fromJSON(const std::string& jsonData);

      bool isValid() const;

   private:
      QString userId_;
      QString contactId_;
      ContactStatus status_;
      QString publicKey_;
      QString displayName_;
      QDateTime publicKeyTime_;

   };
}
#endif // CONTACTRECORDDATA_H
