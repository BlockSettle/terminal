#ifndef __CELER_PROPERTY_H__
#define __CELER_PROPERTY_H__

#include <string>
#include <unordered_map>

struct CelerProperty
{
   CelerProperty(const std::string& propertyName)
      : name(propertyName)
      , id(-1)
   {}
   CelerProperty() : id(-1) {}
   bool empty() const { return name.empty(); }

   std::string name;
   std::string value;
   int64_t     id;
};


typedef std::unordered_map<std::string, CelerProperty>   CelerProperties;


#endif // __CELER_PROPERTY_H__
