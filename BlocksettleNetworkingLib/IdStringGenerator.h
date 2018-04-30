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
