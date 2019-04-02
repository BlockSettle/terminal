#ifndef CHATUSERDATA_H
#define CHATUSERDATA_H

#include "DataObject.h"
#include "../ProtocolDefinitions.h"

namespace Chat {

    class UserData : public DataObject
    {
    public:
       UserData(const QString& userId, UserStatus status);

       QString getUserId();
       UserStatus getUserStatus();

    public:
       QJsonObject toJson() const override;
       static std::shared_ptr<UserData> fromJSON(const std::string& jsonData);

    private:
       QString userId_;
       UserStatus userStatus_;
    };
}

#endif // CHATUSERDATA_H
