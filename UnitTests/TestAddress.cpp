/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <gtest/gtest.h>
#include "Address.h"
#include "BitcoinSettings.h"


TEST(TestAddress, ValidScenarios)
{
   //create random privkey
   auto&& privKey1 = CryptoPRNG::generateRandom(32);
   auto&& privKey2 = CryptoPRNG::generateRandom(32);

   //get pubkey
   auto&& pubkey1 = CryptoECDSA().ComputePublicKey(privKey1, true);
   auto&& pubkey2 = CryptoECDSA().ComputePublicKey(privKey2, true);

   //hash it
   auto&& pubkeyHash1 = BtcUtils::getHash160(pubkey1.getRef());
   auto&& pubkeyHash2 = BtcUtils::getHash160(pubkey2.getRef());

   //randos
   auto&& rando20 = CryptoPRNG::generateRandom(20);
   auto&& rando32 = CryptoPRNG::generateRandom(32);

   {
      //P2PKH
      auto addr = bs::Address::fromPubKey(pubkey1, AddressEntryType_P2PKH);
      
      BinaryData prefixedHash;
      prefixedHash.append(Armory::Config::BitcoinSettings::getPubkeyHashPrefix());
      prefixedHash.append(pubkeyHash1);

      EXPECT_EQ(prefixedHash, addr.prefixed());
      auto b58 = BtcUtils::scrAddrToBase58(prefixedHash);
      EXPECT_EQ(addr.display(), b58);

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(pubkeyHash1, addr2.unprefixed());
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(addr2.display(), b58);

      EXPECT_EQ(addr, addr2);
   }

   {
      //P2WPKH
      auto addr = bs::Address::fromPubKey(pubkey1, AddressEntryType_P2WPKH);

      BinaryData prefixedHash;
      prefixedHash.append(SCRIPT_PREFIX_P2WPKH);
      prefixedHash.append(pubkeyHash1);

      EXPECT_EQ(prefixedHash, addr.prefixed());
      auto bch32 = BtcUtils::scrAddrToSegWitAddress(pubkeyHash1);
      EXPECT_EQ(addr.display(), bch32);

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(pubkeyHash1, addr2.unprefixed());
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(addr2.display(), bch32);
 
      EXPECT_EQ(addr, addr2);
   }

   {
      //P2SH
      BinaryData prefixed;
      prefixed.append(Armory::Config::BitcoinSettings::getScriptHashPrefix());
      prefixed.append(rando20);
      auto addr = bs::Address::fromHash(prefixed);

      BinaryData prefixedHash;
      prefixedHash.append(Armory::Config::BitcoinSettings::getScriptHashPrefix());
      prefixedHash.append(rando20);

      EXPECT_EQ(prefixedHash, addr.prefixed());
      auto b58 = BtcUtils::scrAddrToBase58(prefixedHash);
      EXPECT_EQ(addr.display(), b58);

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(rando20, addr2.unprefixed());
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(addr2.display(), b58);

      EXPECT_EQ(addr, addr2);
   }

   {
      //P2SH - P2PKH
      auto addr = bs::Address::fromPubKey(pubkey1,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PKH));

      BinaryData script = BtcUtils::getP2PKHScript(pubkeyHash1);
      BinaryData prefixedHash;
      prefixedHash.append(Armory::Config::BitcoinSettings::getScriptHashPrefix());
      prefixedHash.append(BtcUtils::getHash160(script));

      EXPECT_EQ(prefixedHash, addr.prefixed());
      auto b58 = BtcUtils::scrAddrToBase58(prefixedHash);
      EXPECT_EQ(b58, addr.display());

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(b58, addr2.display());

      EXPECT_EQ(addr, addr2);
   }

   {
      //P2SH - P2WPKH
      auto addr = bs::Address::fromPubKey(pubkey1,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH));

      BinaryData script = BtcUtils::getP2WPKHOutputScript(pubkeyHash1);
      BinaryData prefixedHash;
      prefixedHash.append(Armory::Config::BitcoinSettings::getScriptHashPrefix());
      prefixedHash.append(BtcUtils::getHash160(script));

      EXPECT_EQ(prefixedHash, addr.prefixed());
      auto b58 = BtcUtils::scrAddrToBase58(prefixedHash);
      EXPECT_EQ(b58, addr.display());

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(b58, addr2.display());

      EXPECT_EQ(addr, addr2);
   }

   {
      //P2SH - P2PK
      auto addr = bs::Address::fromPubKey(pubkey1,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PK));

      BinaryData script = BtcUtils::getP2PKScript(pubkey1);
      BinaryData hash = BtcUtils::getHash160(script);
      EXPECT_EQ(hash, addr.unprefixed());

      BinaryData prefixedHash;
      prefixedHash.append(Armory::Config::BitcoinSettings::getScriptHashPrefix());
      prefixedHash.append(hash);
      EXPECT_EQ(prefixedHash, addr.prefixed());

      auto b58 = BtcUtils::scrAddrToBase58(prefixedHash);
      EXPECT_EQ(b58, addr.display());

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(b58, addr2.display());

      EXPECT_EQ(addr, addr2);
   }

   {
      //P2SH - multisig
      BinaryWriter bw;
      bw.put_uint8_t(OP_1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey2);
      bw.put_uint8_t(OP_2);
      bw.put_uint8_t(OP_CHECKMULTISIG);
      auto& msScript = bw.getData();

      auto addr = bs::Address::fromMultisigScript(msScript, AddressEntryType_P2SH);

      BinaryData hash = BtcUtils::getHash160(msScript);
      EXPECT_EQ(addr.unprefixed(), hash);

      BinaryData prefixedHash;
      prefixedHash.append(Armory::Config::BitcoinSettings::getScriptHashPrefix());
      prefixedHash.append(hash);
      EXPECT_EQ(prefixedHash, addr.prefixed());
      
      auto b58 = BtcUtils::scrAddrToBase58(prefixedHash);
      EXPECT_EQ(b58, addr.display());

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(b58, addr2.display());

      EXPECT_EQ(addr, addr2);
   }

   {
      //P2WSH
      BinaryData prefixed;
      prefixed.append(SCRIPT_PREFIX_P2WSH);
      prefixed.append(rando32);
      auto addr = bs::Address::fromHash(prefixed);
      EXPECT_EQ(rando32, addr.unprefixed());

      BinaryData prefixedHash;
      prefixedHash.append(SCRIPT_PREFIX_P2WSH);
      prefixedHash.append(rando32);

      auto bch32 = BtcUtils::scrAddrToSegWitAddress(rando32);
      EXPECT_EQ(addr.display(), bch32);

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(addr2.display(), bch32);
   
      EXPECT_EQ(addr, addr2);
   }

   {
      //P2WSH - P2WPKH
      auto addr = bs::Address::fromPubKey(pubkey1,
         AddressEntryType(AddressEntryType_P2WSH | AddressEntryType_P2WPKH));

      BinaryData script = BtcUtils::getP2WPKHOutputScript(pubkeyHash1);
      BinaryData hash = BtcUtils::getSha256(script);
      EXPECT_EQ(hash, addr.unprefixed());

      BinaryData prefixedHash;
      prefixedHash.append(SCRIPT_PREFIX_P2WSH);
      prefixedHash.append(hash);
      EXPECT_EQ(prefixedHash, addr.prefixed());

      auto bch32 = BtcUtils::scrAddrToSegWitAddress(hash);
      EXPECT_EQ(bch32, addr.display());

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(addr2.display(), bch32);

      EXPECT_EQ(addr, addr2);
   }

   {
      //P2WSH - multisig
      BinaryWriter bw;
      bw.put_uint8_t(OP_1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey2);
      bw.put_uint8_t(OP_2);
      bw.put_uint8_t(OP_CHECKMULTISIG);
      auto& msScript = bw.getData();

      auto addr = bs::Address::fromMultisigScript(msScript, AddressEntryType_P2WSH);

      BinaryData hash = BtcUtils::getSha256(msScript);
      EXPECT_EQ(addr.unprefixed(), hash);

      BinaryData prefixedHash;
      prefixedHash.append(SCRIPT_PREFIX_P2WSH);
      prefixedHash.append(hash);
      EXPECT_EQ(prefixedHash, addr.prefixed());

      auto bch32 = BtcUtils::scrAddrToSegWitAddress(hash);
      EXPECT_EQ(bch32, addr.display());

      auto addr2 = bs::Address::fromHash(prefixedHash);
      EXPECT_EQ(prefixedHash, addr2.prefixed());
      EXPECT_EQ(addr2.display(), bch32);

      EXPECT_EQ(addr, addr2);
   }
}

TEST(TestAddress, InvalidScenarios)
{
   //create random privkey
   auto&& privKey1 = CryptoPRNG::generateRandom(32);

   //get pubkey
   auto&& pubkey1 = CryptoECDSA().ComputePublicKey(privKey1, true);

   //hash it
   auto&& pubkeyHash1 = BtcUtils::getHash160(pubkey1.getRef());

   //gibberish
   auto&& rando32 = CryptoPRNG::generateRandom(32);
   auto&& rando50 = CryptoPRNG::generateRandom(50);

   {
      //P2WSH - P2PKH
      try
      {
         auto addr = bs::Address::fromPubKey(pubkey1,
            AddressEntryType(AddressEntryType_P2WSH | AddressEntryType_P2PKH));
         ASSERT_TRUE(false);
      }
      catch (std::runtime_error&)
      {
      }
   }

   {
      //from non hash
      try
      {
         auto addr = bs::Address::fromHash(rando50);
         ASSERT_TRUE(false);
      }
      catch (std::runtime_error&)
      {
      }
   }

   {
      //P2SH | P2WSH from pubkey
      try
      {
         auto addr = bs::Address::fromPubKey(rando32,
            AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WSH));
         ASSERT_TRUE(false);
      }
      catch (std::runtime_error&)
      {
      }
   }

   {
      //P2WSH | P2PK from pubkey
      try
      {
         auto addr = bs::Address::fromPubKey(pubkeyHash1,
            AddressEntryType(AddressEntryType_P2WSH | AddressEntryType_P2PK));
         ASSERT_TRUE(false);
      }
      catch (std::runtime_error&)
      {
      }
   }

   {
      //P2WSH | P2PKH from pubkey
      try
      {
         auto addr = bs::Address::fromPubKey(pubkeyHash1,
            AddressEntryType(AddressEntryType_P2WSH | AddressEntryType_P2PKH));
         ASSERT_TRUE(false);
      }
      catch (std::runtime_error&)
      {
      }
   }

   {
      //naked P2PK from pubkey hash
      try
      {
         auto addr = bs::Address::fromPubKey(pubkeyHash1, AddressEntryType_P2PK);
         ASSERT_TRUE(false);
      }
      catch (std::runtime_error&)
      {
      }
   }

   //from hash unprefixed

   //invalid prefix

}

TEST(TestAddress, FromAddress)
{
   //create random privkey
   auto&& privKey = CryptoPRNG::generateRandom(32);
   auto&& privKey2 = CryptoPRNG::generateRandom(32);

   //get pubkey
   auto&& pubkey = CryptoECDSA().ComputePublicKey(privKey, true);
   auto&& pubkey2 = CryptoECDSA().ComputePublicKey(privKey2, true);

   //hash it
   auto&& pubkeyHash = BtcUtils::getHash160(pubkey.getRef());
   auto&& pubkeyHash2 = BtcUtils::getHash160(pubkey2.getRef());

   {
      //p2pkh
      auto addr = bs::Address::fromPubKey(pubkey, AddressEntryType_P2PKH);
      auto addrFromAddr = bs::Address::fromAddressString(addr.display());

      EXPECT_EQ(addrFromAddr.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromAddr.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromAddr.display(), addr.display());
      EXPECT_EQ(addrFromAddr, addr);
   }

   {
      //p2wpkh
      auto addr = bs::Address::fromPubKey(pubkey, AddressEntryType_P2WPKH);
      auto addrFromAddr = bs::Address::fromAddressString(addr.display());

      EXPECT_EQ(addrFromAddr.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromAddr.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromAddr.display(), addr.display());
      EXPECT_EQ(addrFromAddr, addr);
   }

   {
      //naked p2sh
      BinaryData prefixedHash;
      prefixedHash.append(AddressEntry::getPrefixByte(AddressEntryType_P2SH));
      prefixedHash.append(pubkeyHash);
      auto addr = bs::Address::fromHash(prefixedHash);
      auto addrFromAddr = bs::Address::fromAddressString(addr.display());

      EXPECT_EQ(addrFromAddr.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromAddr.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromAddr.display(), addr.display());
      EXPECT_EQ(addrFromAddr, addr);
   }

   {
      //p2sh - p2pk
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PK));
      auto addrFromAddr = bs::Address::fromAddressString(addr.display());

      EXPECT_EQ(addrFromAddr.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromAddr.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromAddr.display(), addr.display());
      EXPECT_EQ(addrFromAddr, addr);
   }

   {
      //p2sh - p2pkh
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PKH));
      auto addrFromAddr = bs::Address::fromAddressString(addr.display());

      EXPECT_EQ(addrFromAddr.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromAddr.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromAddr.display(), addr.display());
      EXPECT_EQ(addrFromAddr, addr);
   }

   {
      //p2sh - p2wpkh
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH));
      auto addrFromAddr = bs::Address::fromAddressString(addr.display());

      EXPECT_EQ(addrFromAddr.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromAddr.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromAddr.display(), addr.display());
   }

   {
      //mock ms script
      BinaryWriter bw;
      bw.put_uint8_t(OP_1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey2);
      bw.put_uint8_t(OP_2);
      bw.put_uint8_t(OP_CHECKMULTISIG);
      auto& msScript = bw.getData();

      //p2wsh | multisig
      auto addr = bs::Address::fromMultisigScript(msScript, AddressEntryType_P2WSH);
      auto addrFromAddr = bs::Address::fromAddressString(addr.display());

      EXPECT_EQ(addrFromAddr.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromAddr.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromAddr.display(), addr.display());
   }
}

TEST(TestAddress, FromRecipient)
{
   //create random privkey
   auto&& privKey = CryptoPRNG::generateRandom(32);
   auto&& privKey2 = CryptoPRNG::generateRandom(32);

   //get pubkey
   auto&& pubkey = CryptoECDSA().ComputePublicKey(privKey, true);
   auto&& pubkey2 = CryptoECDSA().ComputePublicKey(privKey2, true);

   //hash it
   auto&& pubkeyHash = BtcUtils::getHash160(pubkey.getRef());
   auto&& pubkeyHash2 = BtcUtils::getHash160(pubkey2.getRef());

   {
      //p2pkh
      auto addr = bs::Address::fromPubKey(pubkey, AddressEntryType_P2PKH);
      auto recipient = addr.getRecipient(bs::XBTAmount((int64_t)COIN));
      auto addrFromRecipient = bs::Address::fromRecipient(recipient);

      EXPECT_EQ(addrFromRecipient.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromRecipient.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromRecipient.display(), addr.display());
      EXPECT_EQ(addr, addrFromRecipient);
   }

   {
      //p2wpkh
      auto addr = bs::Address::fromPubKey(pubkey, AddressEntryType_P2WPKH);
      auto recipient = addr.getRecipient(bs::XBTAmount((int64_t)COIN));
      auto addrFromRecipient = bs::Address::fromRecipient(recipient);

      EXPECT_EQ(addrFromRecipient.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromRecipient.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromRecipient.display(), addr.display());
      EXPECT_EQ(addr, addrFromRecipient);
   }

   {
      //naked p2sh
      BinaryData prefixed;
      prefixed.append(AddressEntry::getPrefixByte(AddressEntryType_P2SH));
      prefixed.append(CryptoPRNG::generateRandom(20));
      auto addr = bs::Address::fromHash(prefixed);
      auto recipient = addr.getRecipient(bs::XBTAmount((int64_t)COIN));
      auto addrFromRecipient = bs::Address::fromRecipient(recipient);

      EXPECT_EQ(addrFromRecipient.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromRecipient.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromRecipient.display(), addr.display());
      EXPECT_EQ(addr, addrFromRecipient);
   }

   {
      //p2sh - p2pk
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PK));
      auto recipient = addr.getRecipient(bs::XBTAmount((int64_t)COIN));
      auto addrFromRecipient = bs::Address::fromRecipient(recipient);

      EXPECT_EQ(addrFromRecipient.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromRecipient.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromRecipient.display(), addr.display());
      EXPECT_EQ(addr, addrFromRecipient);
   }

   {
      //p2sh - p2pkh
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PKH));
      auto recipient = addr.getRecipient(bs::XBTAmount((int64_t)COIN));
      auto addrFromRecipient = bs::Address::fromRecipient(recipient);

      EXPECT_EQ(addrFromRecipient.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromRecipient.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromRecipient.display(), addr.display());
      EXPECT_EQ(addr, addrFromRecipient);
   }

   {
      //p2sh - p2wpkh
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH));
      auto recipient = addr.getRecipient(bs::XBTAmount((int64_t)COIN));
      auto addrFromRecipient = bs::Address::fromRecipient(recipient);

      EXPECT_EQ(addrFromRecipient.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromRecipient.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromRecipient.display(), addr.display());
      EXPECT_EQ(addr, addrFromRecipient);
   }

   {
      //mock ms script
      BinaryWriter bw;
      bw.put_uint8_t(OP_1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey2);
      bw.put_uint8_t(OP_2);
      bw.put_uint8_t(OP_CHECKMULTISIG);
      auto& msScript = bw.getData();

      //p2wsh | multisig
      auto addr = bs::Address::fromMultisigScript(msScript, AddressEntryType_P2WSH);
      auto recipient = addr.getRecipient(bs::XBTAmount((int64_t)COIN));
      auto addrFromRecipient = bs::Address::fromRecipient(recipient);

      EXPECT_EQ(addrFromRecipient.unprefixed(), addr.unprefixed());
      EXPECT_EQ(addrFromRecipient.prefixed(), addr.prefixed());
      EXPECT_EQ(addrFromRecipient.display(), addr.display());
      EXPECT_EQ(addr, addrFromRecipient);
   }
}

TEST(TestAddress, P2SH_ImplicitDetection)
{
   /*
   This test checks that bs::Address constructed with the aet set to
   naked AddressEntryType_P2SH will treat the data as a script hash.
   */

   //create random script hash
   BinaryData scriptHash;
   scriptHash.append(AddressEntry::getPrefixByte(AddressEntryType_P2SH));
   scriptHash.append(CryptoPRNG::generateRandom(20));

   //instantiate naked P2SH address
   auto addr = bs::Address::fromHash(scriptHash);

   //check prefixed and unprefixed hash match original
   const auto& unprefixed = addr.unprefixed();
   EXPECT_EQ(unprefixed, scriptHash.getSliceCopy(1, 20));

   const auto& prefixed = addr.prefixed();
   ASSERT_EQ(prefixed.getSize(), 21);
   EXPECT_EQ(prefixed, scriptHash);
   EXPECT_EQ(prefixed.getPtr()[0], Armory::Config::BitcoinSettings::getScriptHashPrefix());
}

TEST(TestAddress, P2SH_ExplicitDetection)
{
   /*
   This test checks that bs::Address constructed with the aet set to
   (P2SH | base type) will treat the data as hash pubkey and construct
   the script to hash it correctly.

   It also checks that various different base spit the expected hash.
   */

   //create random privkey
   auto&& privKey = CryptoPRNG::generateRandom(32);

   //get pubkey
   auto&& pubkey = CryptoECDSA().ComputePublicKey(privKey, true);

   //hash it
   auto&& pubkeyHash = BtcUtils::getHash160(pubkey.getRef());

   {
      //P2SH | P2PK
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PK));

      auto&& p2pk_script = BtcUtils::getP2PKScript(pubkey);
      auto&& scriptHash = BtcUtils::getHash160(p2pk_script);

      //check prefixed and unprefixed hash match hashed p2pk script
      const auto& unprefixed = addr.unprefixed();
      EXPECT_EQ(unprefixed, scriptHash);

      const auto& prefixed = addr.prefixed();
      ASSERT_EQ(prefixed.getSize(), 21);
      EXPECT_EQ(prefixed.getSliceCopy(1, 20), scriptHash);
      EXPECT_EQ(prefixed.getPtr()[0], Armory::Config::BitcoinSettings::getScriptHashPrefix());
   }

   {
      //P2SH | P2PKH from pubkey hash
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2PKH));

      auto&& p2pkh_script = BtcUtils::getP2PKHScript(pubkeyHash);
      auto&& scriptHash = BtcUtils::getHash160(p2pkh_script);

      //check prefixed and unprefixed hash match hashed p2pkh script
      const auto& unprefixed = addr.unprefixed();
      EXPECT_EQ(unprefixed, scriptHash);

      const auto& prefixed = addr.prefixed();
      ASSERT_EQ(prefixed.getSize(), 21);
      EXPECT_EQ(prefixed.getSliceCopy(1, 20), scriptHash);
      EXPECT_EQ(prefixed.getPtr()[0], Armory::Config::BitcoinSettings::getScriptHashPrefix());
   }

   {
      //P2SH | P2WPKH from pubkey hash
      auto addr = bs::Address::fromPubKey(pubkey,
         AddressEntryType(AddressEntryType_P2SH | AddressEntryType_P2WPKH));

      auto&& p2wpkh_script = BtcUtils::getP2WPKHOutputScript(pubkeyHash);
      auto&& scriptHash = BtcUtils::getHash160(p2wpkh_script);

      //check prefixed and unprefixed hash match hashed p2wpkh script
      const auto& unprefixed = addr.unprefixed();
      EXPECT_EQ(unprefixed, scriptHash);

      const auto& prefixed = addr.prefixed();
      ASSERT_EQ(prefixed.getSize(), 21);
      EXPECT_EQ(prefixed.getSliceCopy(1, 20), scriptHash);
      EXPECT_EQ(prefixed.getPtr()[0], Armory::Config::BitcoinSettings::getScriptHashPrefix());
   }
}

TEST(TestAddress, P2WSH_ImplicitDetection)
{
   //create random script hash
   BinaryData scriptHash;
   scriptHash.append(AddressEntry::getPrefixByte(AddressEntryType_P2WSH));
   scriptHash.append(CryptoPRNG::generateRandom(32));

   //instantiate naked P2SH address
   auto addr = bs::Address::fromHash(scriptHash);

   //check prefixed and unprefixed hash match original
   const auto& unprefixed = addr.unprefixed();
   EXPECT_EQ(unprefixed, scriptHash.getSliceCopy(1, 32));

   const auto& prefixed = addr.prefixed();
   ASSERT_EQ(prefixed.getSize(), 33);
   EXPECT_EQ(prefixed, scriptHash);
   EXPECT_EQ(prefixed.getPtr()[0], SCRIPT_PREFIX_P2WSH);
}

TEST(TestAddress, P2WSH_ExplicitDetection)
{
   //create random privkey
   auto&& privKey1 = CryptoPRNG::generateRandom(32);
   auto&& privKey2 = CryptoPRNG::generateRandom(32);

   //get pubkey
   auto&& pubkey1 = CryptoECDSA().ComputePublicKey(privKey1, true);
   auto&& pubkey2 = CryptoECDSA().ComputePublicKey(privKey2, true);

   //hash it
   auto&& pubkeyHash1 = BtcUtils::getHash160(pubkey1.getRef());
   auto&& pubkeyHash2 = BtcUtils::getHash160(pubkey2.getRef());

   {
      //P2WSH | P2WPKH
      auto addr = bs::Address::fromPubKey(pubkey1,
         AddressEntryType(AddressEntryType_P2WSH | AddressEntryType_P2WPKH));

      auto&& p2pkh_script = BtcUtils::getP2WPKHOutputScript(pubkeyHash1);
      auto&& scriptHash = BtcUtils::BtcUtils::getSha256(p2pkh_script);

      //check prefixed and unprefixed hash match hashed p2wsh script
      const auto& unprefixed = addr.unprefixed();
      EXPECT_EQ(unprefixed, scriptHash);

      const auto& prefixed = addr.prefixed();
      ASSERT_EQ(prefixed.getSize(), 33);
      EXPECT_EQ(prefixed.getSliceCopy(1, 32), scriptHash);
      EXPECT_EQ(prefixed.getPtr()[0], SCRIPT_PREFIX_P2WSH);
   }

   {
      //P2WSH | multisig script

      //mock ms script, this doesn't have to be valid
      BinaryWriter bw;
      bw.put_uint8_t(OP_1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey1);
      bw.put_uint8_t(33);
      bw.put_BinaryData(pubkey2);
      bw.put_uint8_t(OP_2);
      bw.put_uint8_t(OP_CHECKMULTISIG);
      auto& msScript = bw.getData();

      auto addr = bs::Address::fromMultisigScript(msScript, AddressEntryType_P2WSH);

      auto&& scriptHash = BtcUtils::BtcUtils::getSha256(msScript);

      //check prefixed and unprefixed hash match hashed p2wsh script
      const auto& unprefixed = addr.unprefixed();
      EXPECT_EQ(unprefixed, scriptHash);

      const auto& prefixed = addr.prefixed();
      ASSERT_EQ(prefixed.getSize(), 33);
      EXPECT_EQ(prefixed.getSliceCopy(1, 32), scriptHash);
      EXPECT_EQ(prefixed.getPtr()[0], SCRIPT_PREFIX_P2WSH);
   }
}
