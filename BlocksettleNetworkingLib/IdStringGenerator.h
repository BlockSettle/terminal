/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ID_STRING_GENERATOR_H__
#define __ID_STRING_GENERATOR_H__

#include <atomic>
#include <string>

class IdStringGenerator
{
public:
   IdStringGenerator();
   ~IdStringGenerator() noexcept = default;

   IdStringGenerator(const IdStringGenerator&) = delete;
   IdStringGenerator& operator = (const IdStringGenerator&) = delete;

   IdStringGenerator(IdStringGenerator&&) = delete;
   IdStringGenerator& operator = (IdStringGenerator&&) = delete;

   std::string getNextId();
   std::string getUniqueSeed() const;

   void setUserName(const std::string &uname) { userName_ = uname; }

private:
   std::atomic_uint        id_;
   std::string             userName_;
};

#endif // __ID_STRING_GENERATOR_H__
