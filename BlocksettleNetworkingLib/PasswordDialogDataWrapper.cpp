#include "PasswordDialogDataWrapper.h"
#include "google/protobuf/any.h"
#include "google/protobuf/map.h"

using namespace google::protobuf;
using namespace Blocksettle::Communication::Internal;

const std::string kTypeErrorMsg = "PasswordDialogData value read error: wrong type";

void PasswordDialogDataWrapper::insert(const bs::sync::dialog::keys::Key &key, bool value) { insertImpl<bool>(key.toString(), value); }

void PasswordDialogDataWrapper::insert(const bs::sync::dialog::keys::Key &key, const std::string &value)
{
   insertImpl<std::string>(key.toString(), value);
}

void PasswordDialogDataWrapper::insert(const bs::sync::dialog::keys::Key &key, int value) { insertImpl<int>(key.toString(), value); }

void PasswordDialogDataWrapper::insert(const bs::sync::dialog::keys::Key &key, double value) { insertImpl<double>(key.toString(), value); }

void PasswordDialogDataWrapper::insert(const bs::sync::dialog::keys::Key &key, const char *data, size_t size)
{
   AnyMessage msg;
   msg.set_value_bytes(data, size);

   Any any;
   any.PackFrom(msg);
   const auto &p = MapPair<std::string, Any>(key.toString(), any);
   mutable_valuesmap()->insert(p);
}

static AnyMessage &setValueImpl(AnyMessage &anyMsg, bool value)
{
   anyMsg.set_value_bool(value);
   return anyMsg;
}

static AnyMessage &setValueImpl(AnyMessage &anyMsg, const std::string &value)
{
   anyMsg.set_value_string(value);
   return anyMsg;
}

static AnyMessage &setValueImpl(AnyMessage &anyMsg, int32 value)
{
   anyMsg.set_value_int32(value);
   return anyMsg;
}

static AnyMessage &setValueImpl(AnyMessage &anyMsg, double value)
{
   anyMsg.set_value_double(value);
   return anyMsg;
}

template<typename T>
inline void PasswordDialogDataWrapper::insertImpl(const std::string &key, T value)
{
   AnyMessage msg;
   setValueImpl(msg, value);

   Any any;
   any.PackFrom(msg);

   (*mutable_valuesmap())[key] = any;
}

///

template<>
bool PasswordDialogDataWrapper::valueImpl<bool>(const AnyMessage &anyMsg) const
{
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueBool) {
      return false;
   }
   return anyMsg.value_bool();
}

template<>
std::string PasswordDialogDataWrapper::valueImpl<std::string>(const AnyMessage &anyMsg) const
{
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueString) {
      return std::string();
   }
   return anyMsg.value_string();
}

template<>
int PasswordDialogDataWrapper::valueImpl<int>(const AnyMessage &anyMsg) const
{
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueInt32) {
      return 0;
   }
   return anyMsg.value_int32();
}

template<>
double PasswordDialogDataWrapper::valueImpl<double>(const AnyMessage &anyMsg) const
{
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueDouble) {
      return 0;
   }
   return anyMsg.value_double();
}

template<>
const char * PasswordDialogDataWrapper::valueImpl<const char *>(const AnyMessage &anyMsg) const
{
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueBytes) {
      return nullptr;
   }
   return anyMsg.value_bytes().data();
}


