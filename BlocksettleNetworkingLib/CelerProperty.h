/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
