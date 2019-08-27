#include "PasswordDialogDataWrapper.h"
#include "google/protobuf/any.h"
#include "google/protobuf/map.h"

using namespace google::protobuf;
using namespace Blocksettle::Communication::Internal;

const std::string kTypeErrorMsg = "PasswordDialogData value read error: wrong type";

void PasswordDialogDataWrapper::insert(const std::string &key, bool value) { insertImpl<bool>(key, value); }

void PasswordDialogDataWrapper::insert(const std::string &key, const std::string &value)
{
   insertImpl<std::string>(key, value);
}

void PasswordDialogDataWrapper::insert(const std::string &key, int value) { insertImpl<int>(key, value); }

void PasswordDialogDataWrapper::insert(const std::string &key, double value) { insertImpl<double>(key, value); }

void PasswordDialogDataWrapper::insert(const std::string &key, const char *data, size_t size)
{
   AnyMessage msg;
   msg.set_value_bytes(data, size);

   Any any;
   any.PackFrom(msg);
   const auto &p = MapPair<std::string, Any>(key, any);
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

   const auto &p = MapPair<std::string, Any>(key, any);
   mutable_valuesmap()->insert(p);
}

/*template<typename T>
AnyMessage &PasswordDialogDataWrapper::setValueImpl(AnyMessage &anyMsg, T)
{
   assert(false);
   return anyMsg;
}*/

template<>
bool PasswordDialogDataWrapper::value<bool>(const std::string &key) const
{
   const Any &msg = valuesmap().at(key);
   AnyMessage anyMsg;
   msg.UnpackTo(&anyMsg);
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueBool) {
      throw std::runtime_error(kTypeErrorMsg);
   }
   return anyMsg.value_bool();
}

template<>
std::string PasswordDialogDataWrapper::value<std::string>(const std::string &key) const
{
   const Any &msg = valuesmap().at(key);
   AnyMessage anyMsg;
   msg.UnpackTo(&anyMsg);
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueString) {
      throw std::runtime_error(kTypeErrorMsg);
   }
   return anyMsg.value_string();
}

template<>
int PasswordDialogDataWrapper::value<int>(const std::string &key) const
{
   const Any &msg = valuesmap().at(key);
   AnyMessage anyMsg;
   msg.UnpackTo(&anyMsg);
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueInt32) {
      throw std::runtime_error(kTypeErrorMsg);
   }
   return anyMsg.value_int32();
}

template<>
double PasswordDialogDataWrapper::value<double>(const std::string &key) const
{
   const Any &msg = valuesmap().at(key);
   AnyMessage anyMsg;
   msg.UnpackTo(&anyMsg);
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueBool) {
      throw std::runtime_error(kTypeErrorMsg);
   }
   return anyMsg.value_double();
}

template<>
const char * PasswordDialogDataWrapper::value<const char *>(const std::string &key) const
{
   const Any &msg = valuesmap().at(key);
   AnyMessage anyMsg;
   msg.UnpackTo(&anyMsg);
   if (anyMsg.value_case() != Blocksettle::Communication::Internal::AnyMessage::ValueCase::kValueBool) {
      throw std::runtime_error(kTypeErrorMsg);
   }
   return anyMsg.value_bytes().data();
}

