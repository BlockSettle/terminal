#ifndef CONTACTRECORDDATA_H
#define CONTACTRECORDDATA_H

#include "DataObject.h"
#include "../ProtocolDefinitions.h"

namespace Chat {
    class ContactRecordData : public DataObject
    {
    public:
       ContactRecordData(const QString& userId, const QString& contactId, ContactStatus status, autheid::PublicKey publicKey);

       // DataObject interface
       QString getContactForId();
       QString getContactId();
       ContactStatus getContactStatus();
       autheid::PublicKey getContactPublicKey();

    public:
       QJsonObject toJson() const override;
       static std::shared_ptr<ContactRecordData> fromJSON(const std::string& jsonData);
    private:
       QString userId_;
       QString contactId_;
       ContactStatus status_;
       autheid::PublicKey publicKey_;

    };
}
#endif // CONTACTRECORDDATA_H
