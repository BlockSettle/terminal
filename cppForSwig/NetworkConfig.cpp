////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2018, goatpig                                               //            
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                   
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "NetworkConfig.h"

using namespace std;

uint8_t NetworkConfig::pubkeyHashPrefix_;
uint8_t NetworkConfig::scriptHashPrefix_;
BinaryData NetworkConfig::genesisBlockHash_;
BinaryData NetworkConfig::genesisTxHash_;
BinaryData NetworkConfig::magicBytes_;
NETWORK_MODE NetworkConfig::mode_;

////////////////////////////////////////////////////////////////////////////////
void NetworkConfig::selectNetwork(NETWORK_MODE mode)
{
   switch (mode)
   {
   case NETWORK_MODE_MAINNET:
   {
      genesisBlockHash_ = READHEX(MAINNET_GENESIS_HASH_HEX);
      genesisTxHash_ = READHEX(MAINNET_GENESIS_TX_HASH_HEX);
      magicBytes_ = READHEX(MAINNET_MAGIC_BYTES);
      pubkeyHashPrefix_ = SCRIPT_PREFIX_HASH160;
      scriptHashPrefix_ = SCRIPT_PREFIX_P2SH;

      break;
   }

   case NETWORK_MODE_TESTNET:
   {
      genesisBlockHash_ = READHEX(TESTNET_GENESIS_HASH_HEX);
      genesisTxHash_ = READHEX(TESTNET_GENESIS_TX_HASH_HEX);
      magicBytes_ = READHEX(TESTNET_MAGIC_BYTES);
      pubkeyHashPrefix_ = SCRIPT_PREFIX_HASH160_TESTNET;
      scriptHashPrefix_ = SCRIPT_PREFIX_P2SH_TESTNET;

      break;
   }

   case NETWORK_MODE_REGTEST:
   {
      genesisBlockHash_ = READHEX(REGTEST_GENESIS_HASH_HEX);
      genesisTxHash_ = READHEX(REGTEST_GENESIS_TX_HASH_HEX);
      magicBytes_ = READHEX(REGTEST_MAGIC_BYTES);
      pubkeyHashPrefix_ = SCRIPT_PREFIX_HASH160_TESTNET;
      scriptHashPrefix_ = SCRIPT_PREFIX_P2SH_TESTNET;

      break;
   }

   default:
      mode_ = NETWORK_MODE_NA;
      LOGERR << "invalid network mode selection";
      throw runtime_error("invalid network mode selection");
   }

   mode_ = mode;
}

////////////////////////////////////////////////////////////////////////////////
bool NetworkConfig::isInitialized()
{
   return mode_ != NETWORK_MODE_NA;
}

////////////////////////////////////////////////////////////////////////////////
uint8_t NetworkConfig::getPubkeyHashPrefix(void)
{ 
   if (!isInitialized())
   {
      LOGERR << "NetworkConfig is uninitialized!";
      throw runtime_error("NetworkConfig is uninitialized!");
   }

   return pubkeyHashPrefix_; 
}

////////////////////////////////////////////////////////////////////////////////
uint8_t NetworkConfig::getScriptHashPrefix(void) 
{ 
   if (!isInitialized())
   {
      LOGERR << "NetworkConfig is uninitialized!";
      throw runtime_error("NetworkConfig is uninitialized!");
   }

   return scriptHashPrefix_; 
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& NetworkConfig::getGenesisBlockHash(void) 
{
   if (!isInitialized())
   {
      LOGERR << "NetworkConfig is uninitialized!";
      throw runtime_error("NetworkConfig is uninitialized!");
   }

   return genesisBlockHash_; 
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& NetworkConfig::getGenesisTxHash(void) 
{
   if (!isInitialized())
   {
      LOGERR << "NetworkConfig is uninitialized!";
      throw runtime_error("NetworkConfig is uninitialized!");
   }

   return genesisTxHash_; 
}

////////////////////////////////////////////////////////////////////////////////
const BinaryData& NetworkConfig::getMagicBytes(void) 
{
   if (!isInitialized())
   {
      LOGERR << "NetworkConfig is uninitialized!";
      throw runtime_error("NetworkConfig is uninitialized!");
   }

   return magicBytes_; 
}
