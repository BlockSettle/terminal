/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_COMMAND_SEQUENCE_H__
#define __CELER_COMMAND_SEQUENCE_H__

#include <cassert>
#include <stdexcept>
#include <string>
#include <vector>

#include "CelerMessageMapper.h"

struct CelerMessage
{
   CelerAPI::CelerMessageType messageType;
   std::string                messageData;
};

// base class introduced just to store derived template classes in same collection
class BaseCelerCommand
{
public:
   BaseCelerCommand(const std::string& name) : name_(name) {}
   virtual ~BaseCelerCommand() noexcept = default;

   BaseCelerCommand(const BaseCelerCommand&) = delete;
   BaseCelerCommand& operator = (const BaseCelerCommand&) = delete;

   BaseCelerCommand(BaseCelerCommand&&) = delete;
   BaseCelerCommand& operator = (BaseCelerCommand&&) = delete;

   std::string GetCommandName() const { return name_; }
public:
   virtual bool IsCompleted() const = 0;
   virtual bool IsSequenceFailed() const = 0;
   virtual bool IsWaitingForData() const = 0;

   virtual CelerMessage GetNextDataToSend() = 0;
   virtual bool OnMessage(const CelerMessage& data) = 0;

   virtual bool FinishSequence() = 0;

   inline void SetSequenceId(const std::string &id) { id_ = id; }
   inline std::string GetSequenceId() const { return id_; }

   void SetUniqueSeed(const std::string &seed) { seed_ = seed; }
   std::string GetUniqueId() const { return seed_ + "/" + std::to_string(uniqCnt_++); }

private:
   std::string id_, seed_;
   mutable unsigned int uniqCnt_ = 0;
   const std::string name_;
};


template<class _C>
class CelerCommandSequence : public BaseCelerCommand
{
public:
   using onDataFunction = bool (_C::*)(const CelerMessage& message);
   using getDataFunction = CelerMessage (_C::*)();

   struct CelerSequenceStep
   {
      bool receiveStep;
      onDataFunction  onMessage;
      getDataFunction getData;
   };
public:
   CelerCommandSequence( const std::string& name, const std::vector<CelerSequenceStep>& steps)
      : BaseCelerCommand(name)
      , currentStep_(0)
      , steps_(steps)
      , stepFailed_(false)
   {}

   ~CelerCommandSequence() noexcept override = default;

   CelerCommandSequence(const CelerCommandSequence&) = delete;
   CelerCommandSequence& operator = (const CelerCommandSequence&) = delete;

   CelerCommandSequence(CelerCommandSequence&&) = delete;
   CelerCommandSequence& operator = (CelerCommandSequence&&) = delete;

public:
   bool IsCompleted() const override
   {
      return IsSequenceFailed() || steps_.empty() || (currentStep_ == steps_.size());
   }

   bool IsSequenceFailed() const override
   {
      return stepFailed_;
   }

   bool IsWaitingForData() const override
   {
      return steps_[currentStep_].receiveStep;
   }

   CelerMessage GetNextDataToSend() override
   {
      assert(!stepFailed_);
      assert(!IsCompleted());
      assert(steps_[currentStep_].getData != nullptr);
      assert(steps_[currentStep_].onMessage == nullptr);

      return ( (_C*)(this)->*steps_[currentStep_++].getData)();
   }

   bool OnMessage(const CelerMessage& data) override
   {
      assert(!stepFailed_);
      assert(!IsCompleted());
      assert(steps_[currentStep_].getData == nullptr);
      assert(steps_[currentStep_].onMessage != nullptr);

      if ( !( (_C*)(this)->*steps_[currentStep_++].onMessage)(data)) {
         stepFailed_ = true;
         return false;
      }

      return true;
   }

private:
   size_t                           currentStep_;
   std::vector<CelerSequenceStep>   steps_;
   bool                             stepFailed_;
};

#endif // __CELER_COMMAND_SEQUENCE_H__
