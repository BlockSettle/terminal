#ifndef CONTACTRECORDDATA_H
#define CONTACTRECORDDATA_H

#include "DataObject.h"
#include "../ProtocolDefinitions.h"

namespace Chat {
    class ContactRecordData : public DataObject
    {
    public:
       ContactRecordData(const QString& userId
                         , const QString& contactId
                         , ContactStatus status
                         , autheid::PublicKey publicKey
                         , const QString& displayName = QString());

       QString getContactForId();
       QString getContactId();
       QString getContactDisplayName();
       ContactStatus getContactStatus();
       autheid::PublicKey getContactPublicKey();

    public:
       QJsonObject toJson() const override;
       static std::shared_ptr<ContactRecordData> fromJSON(const std::string& jsonData);
       void setStatus(const ContactStatus &status);

       QString getDisplayName() const;
       void setDisplayName(const QString &displayName);
       bool hasDisplayName() const;

       void setUserId(const QString &userId);

    private:
       QString userId_;
       QString contactId_;
       ContactStatus status_;
       autheid::PublicKey publicKey_;
       QString displayName_;

    };
}
#endif // CONTACTRECORDDATA_H
