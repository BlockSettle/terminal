/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_SET_USER_PROPERTY_SEQUENCE_H__
#define __CELER_SET_USER_PROPERTY_SEQUENCE_H__

#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}

#include "CelerCommandSequence.h"

#include "CelerProperty.h"

class CelerSetUserPropertySequence : public CelerCommandSequence<CelerSetUserPropertySequence>
{
public:
   using callback_func = std::function<void (bool)>;

public:
   CelerSetUserPropertySequence(const std::shared_ptr<spdlog::logger>& logger
      , const std::string& username
      , const CelerProperty& property);
   ~CelerSetUserPropertySequence() noexcept = default;

   CelerSetUserPropertySequence(const CelerSetUserPropertySequence&) = delete;
   CelerSetUserPropertySequence& operator = (const CelerSetUserPropertySequence&) = delete;

   CelerSetUserPropertySequence(CelerSetUserPropertySequence&&) = delete;
   CelerSetUserPropertySequence& operator = (CelerSetUserPropertySequence&&) = delete;

   void SetCallback(const callback_func& callback) {
      callback_ = callback;
   }

   bool FinishSequence() override {
      if (callback_) {
         callback_(result_);
      }
      return result_;
   }

private:
   CelerMessage sendSetPropertyRequest();

   bool processSetPropertyResponse(const CelerMessage& message);

private:
   std::shared_ptr<spdlog::logger> logger_;

   std::string    userName_;
   CelerProperty  property_;

   bool           result_;
   callback_func  callback_;
};

#endif // __CELER_SET_USER_PROPERTY_SEQUENCE_H__
