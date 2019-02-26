////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2019, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_AUTHORIZEDPEERS
#define _H_AUTHORIZEDPEERS

#include <memory>
#include <string>
#include <map>
#include "btc/ecc_key.h"
#include "EncryptionUtils.h"
#include "Wallets.h"
#include "DBUtils.h"
#include "BlockDataManagerConfig.h"

#define PEERS_WALLET_PASSWORD "password"
#define PEERS_WALLET_BIP32_ACCOUNT 0xFF005618

////////////////////////////////////////////////////////////////////////////////
class PeerFileMissing
{
public:
   PeerFileMissing(void)
   {}
};

////////////////////////////////////////////////////////////////////////////////
class AuthorizedPeersException : public std::runtime_error
{
public:
   AuthorizedPeersException(const std::string& err) : 
      std::runtime_error(err)
   {}
};

////////////////////////////////////////////////////////////////////////////////
class AuthorizedPeers
{
private:
   std::map<std::string, btc_pubkey> nameToKeyMap_;
   std::set<SecureBinaryData> keySet_;
   std::map<BinaryData, SecureBinaryData> privateKeys_;

   //for wallet management
   std::map<SecureBinaryData, std::set<unsigned>> keyToAssetIndexMap_;

   std::shared_ptr<AssetWallet> wallet_ = nullptr;

private:
   void loadWallet(const std::string&);
   void createWallet(const std::string&, const std::string&);

   void addPeer(
      const SecureBinaryData&, const std::initializer_list<std::string>&);
   void addPeer(
      const btc_pubkey&, const std::initializer_list<std::string>&);

public:
   AuthorizedPeers(const std::string&, const std::string&);
   AuthorizedPeers(void);

   const std::map<std::string, btc_pubkey>& getPeerNameMap(void) const;
   const std::set<SecureBinaryData>& getPublicKeySet(void) const;
   const SecureBinaryData& getPrivateKey(const BinaryDataRef&) const;

   /* addPeer:
   input:
   - pubkey as SecurBinaryData/btc_pubkey. secp256k1 un/compressed public key
   - count as unsigned: number of names as strings, at least 1
   - count names as string/char*
   */
   template<typename... Types>
   void addPeer(const SecureBinaryData& pubkey, const Types... strings)
   {
      //variadic template shenanigans. This makes sure we get compiler errors 
      //if the wrong arg types are passed (something that can't natively 
      //convert to std::string), instead of blowing up at runtime
      std::initializer_list<std::string> names({ strings... });
      addPeer(pubkey, names);
   }

   template<typename... Types>
   void addPeer(const btc_pubkey& pubkey, const Types... strings)
   {
      std::initializer_list<std::string> names({ strings... });
      addPeer(pubkey, names);
   }

   void addPeer(
      const SecureBinaryData&, const std::vector<std::string>&);

   //
   void eraseName(const std::string&);
   void eraseKey(const SecureBinaryData&);
   void eraseKey(const btc_pubkey&);

   const btc_pubkey& getOwnPublicKey(void) const;
};

#endif
