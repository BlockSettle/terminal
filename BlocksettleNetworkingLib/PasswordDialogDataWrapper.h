#ifndef __PASSWORD_DIALOG_DATA_WRAPPER_H__
#define __PASSWORD_DIALOG_DATA_WRAPPER_H__

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

   void insert(const std::string &key, bool value);
   void insert(const std::string &key, const std::string &value);
   void insert(const std::string &key, int value);
   void insert(const std::string &key, double value);
   void insert(const std::string &key, const char *data, size_t size);

private:
   template<typename T>
   void insertImpl(const std::string &key, T value);
};


} // namespace Internal
} // namespace Communication
} // Blocksettle
#endif // __PASSWORD_DIALOG_DATA_WRAPPER_H__
