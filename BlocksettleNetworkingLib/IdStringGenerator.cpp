#include "IdStringGenerator.h"
#include "EncryptionUtils.h"


IdStringGenerator::IdStringGenerator() : id_(1)
{}

std::string IdStringGenerator::getNextId()
{
   std::stringstream ss;
   ss << std::hex << id_++;
   return ss.str();
}

std::string IdStringGenerator::getUniqueSeed() const
{
   return userName_ + "/" + CryptoPRNG::generateRandom(4).toHexStr();
}
