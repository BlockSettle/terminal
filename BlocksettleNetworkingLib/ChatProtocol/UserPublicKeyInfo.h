#ifndef USERPUBLICKEYINFO_H
#define USERPUBLICKEYINFO_H

#include <memory>
#include <vector>

#include <QMetaType>
#include <QString>
#include <QDateTime>

namespace Chat
{
   class UserPublicKeyInfo;

   using UserPublicKeyInfoPtr = std::shared_ptr<UserPublicKeyInfo>;
   using UserPublicKeyInfoList = std::vector<UserPublicKeyInfoPtr>;

   class UserPublicKeyInfo
   {
   public:
      UserPublicKeyInfo()
      {
         qRegisterMetaType<Chat::UserPublicKeyInfoPtr>();
         qRegisterMetaType<Chat::UserPublicKeyInfoList>();
      }

      QString user_hash() const { return user_hash_; }
      void setUser_hash(QString val) { user_hash_ = val; }

      QString oldPublicKeyHex() const { return oldPublicKeyHex_; }
      void setOldPublicKeyHex(QString val) { oldPublicKeyHex_ = val; }

      QDateTime oldPublicKeyTime() const { return oldPublicKeyTime_; }
      void setOldPublicKeyTime(QDateTime val) { oldPublicKeyTime_ = val; }

      QString newPublicKeyHex() const { return newPublicKeyHex_; }
      void setNewPublicKeyHex(QString val) { newPublicKeyHex_ = val; }

      QDateTime newPublicKeyTime() const { return newPublicKeyTime_; }
      void setNewPublicKeyTime(QDateTime val) { newPublicKeyTime_ = val; }

   private:
      QString user_hash_;
      QString oldPublicKeyHex_;
      QDateTime oldPublicKeyTime_;
      QString newPublicKeyHex_;
      QDateTime newPublicKeyTime_;
   };

}

Q_DECLARE_METATYPE(Chat::UserPublicKeyInfoPtr)
Q_DECLARE_METATYPE(Chat::UserPublicKeyInfoList)

#endif // USERPUBLICKEYINFO_H
