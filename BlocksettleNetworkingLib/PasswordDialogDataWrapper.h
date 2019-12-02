/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __PASSWORD_DIALOG_DATA_WRAPPER_H__
#define __PASSWORD_DIALOG_DATA_WRAPPER_H__

#include "PasswordDialogData.h"
#include "Blocksettle_Communication_Internal.pb.h"

namespace Blocksettle {
namespace Communication {
namespace Internal {

class PasswordDialogDataWrapper : public PasswordDialogData
{
public:
   PasswordDialogDataWrapper() : PasswordDialogData() {}

   // copy constructors and operator= uses parent implementation
   PasswordDialogDataWrapper(const PasswordDialogData &seed) : PasswordDialogData(seed){}
   PasswordDialogDataWrapper(const PasswordDialogDataWrapper &other) : PasswordDialogData(static_cast<PasswordDialogData>(other)) {}
   PasswordDialogDataWrapper& operator= (const PasswordDialogData &other) { PasswordDialogData::operator=(other); return *this;}

   template<typename T>
   T value(const bs::sync::dialog::keys::Key &key) const noexcept
   {
      return value<T>(key.toString());
   }

   void insert(const bs::sync::dialog::keys::Key &key, bool value);
   void insert(const bs::sync::dialog::keys::Key &key, const std::string &value);
   void insert(const bs::sync::dialog::keys::Key &key, int value);
   void insert(const bs::sync::dialog::keys::Key &key, double value);
   void insert(const bs::sync::dialog::keys::Key &key, const char *data, size_t size);

private:
   template<typename T>
   T value(const std::string &key) const noexcept
   {
      try {
         if (!valuesmap().contains(key)) {
            return T();
         }

         const google::protobuf::Any &msg = valuesmap().at(key);
         Blocksettle::Communication::Internal::AnyMessage anyMsg;
         msg.UnpackTo(&anyMsg);

         return valueImpl<T>(anyMsg);
      } catch (...) {
         return  T();
      }
   }

   template<typename T>
   void insertImpl(const std::string &key, T value);

   template<typename T>
   T valueImpl(const AnyMessage &anyMsg) const;
};


template<> bool PasswordDialogDataWrapper::valueImpl<bool>(const AnyMessage &anyMsg) const;
template<> std::string PasswordDialogDataWrapper::valueImpl<std::string>(const AnyMessage &anyMsg) const;
template<> int PasswordDialogDataWrapper::valueImpl<int>(const AnyMessage &anyMsg) const;
template<> double PasswordDialogDataWrapper::valueImpl<double>(const AnyMessage &anyMsg) const;
template<> const char * PasswordDialogDataWrapper::valueImpl<const char *>(const AnyMessage &anyMsg) const;

} // namespace Internal
} // namespace Communication
} // Blocksettle
#endif // __PASSWORD_DIALOG_DATA_WRAPPER_H__
