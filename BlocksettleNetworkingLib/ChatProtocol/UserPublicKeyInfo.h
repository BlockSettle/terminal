/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef USERPUBLICKEYINFO_H
#define USERPUBLICKEYINFO_H

#include <memory>
#include <vector>

#include <QMetaType>
#include <QString>
#include <QDateTime>

#include <disable_warnings.h>
#include "BinaryData.h"
#include <enable_warnings.h>

namespace Chat
{
   class UserPublicKeyInfo;

   using UserPublicKeyInfoPtr = std::shared_ptr<UserPublicKeyInfo>;
   using UserPublicKeyInfoList = std::vector<UserPublicKeyInfoPtr>;

   class UserPublicKeyInfo
   {
   public:
      QString user_hash() const { return user_hash_; }
      void setUser_hash(const QString& val) { user_hash_ = val; }

      BinaryData oldPublicKey() const { return oldPublicKey_; }
      void setOldPublicKeyHex(const BinaryData& val) { oldPublicKey_ = val; }

      QDateTime oldPublicKeyTime() const { return oldPublicKeyTime_; }
      void setOldPublicKeyTime(const QDateTime& val) { oldPublicKeyTime_ = val; }

      BinaryData newPublicKey() const { return newPublicKey_; }
      void setNewPublicKeyHex(const BinaryData& val) { newPublicKey_ = val; }

      QDateTime newPublicKeyTime() const { return newPublicKeyTime_; }
      void setNewPublicKeyTime(const QDateTime& val) { newPublicKeyTime_ = val; }

   private:
      QString user_hash_;
      BinaryData oldPublicKey_;
      QDateTime oldPublicKeyTime_;
      BinaryData newPublicKey_;
      QDateTime newPublicKeyTime_;
   };

}

Q_DECLARE_METATYPE(Chat::UserPublicKeyInfoPtr)
Q_DECLARE_METATYPE(Chat::UserPublicKeyInfoList)

#endif // USERPUBLICKEYINFO_H
