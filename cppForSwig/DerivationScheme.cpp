////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "ReentrantLock.h"
#include "DerivationScheme.h"
#include "DecryptedDataContainer.h"

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//// DerivationScheme
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
DerivationScheme::~DerivationScheme()
{}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<DerivationScheme> DerivationScheme::deserialize(BinaryDataRef data)
{
   BinaryRefReader brr(data);

   //get derivation scheme type
   auto schemeType = brr.get_uint8_t();

   shared_ptr<DerivationScheme> derScheme;

   switch (schemeType)
   {
   case DERIVATIONSCHEME_LEGACY:
   {
      //get chaincode;
      auto len = brr.get_var_int();
      auto&& chainCode = SecureBinaryData(brr.get_BinaryDataRef(len));
      derScheme = make_shared<DerivationScheme_ArmoryLegacy>(
         chainCode);

      break;
   }

   case DERIVATIONSCHEME_MULTISIG:
   {
      //grab n, m
      auto m = brr.get_uint32_t();
      auto n = brr.get_uint32_t();

      set<BinaryData> ids;
      while (brr.getSizeRemaining() > 0)
      {
         auto len = brr.get_var_int();
         auto&& id = brr.get_BinaryData(len);
         ids.insert(move(id));
      }

      if (ids.size() != n)
         throw DerivationSchemeDeserException("id count mismatch");

      //derScheme = make_shared<DerivationScheme_Multisig>(ids, n, m);

      break;
   }

   default:
      throw DerivationSchemeDeserException("unsupported derivation scheme");
   }

   return derScheme;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single> 
   DerivationScheme_ArmoryLegacy::computeNextPublicEntry(
   const SecureBinaryData& pubKey,
   const BinaryData& accountID, unsigned index)
{
   auto&& nextPubkey = CryptoECDSA().ComputeChainedPublicKey(
      pubKey, chainCode_, nullptr);

   return make_shared<AssetEntry_Single>(
      index, accountID,
      nextPubkey, nullptr);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> DerivationScheme_ArmoryLegacy::extendPublicChain(
   shared_ptr<AssetEntry> firstAsset, unsigned count)
{
   auto nextAsset = [this](
      shared_ptr<AssetEntry> assetPtr)->shared_ptr<AssetEntry>
   {
      auto assetSingle =
         dynamic_pointer_cast<AssetEntry_Single>(assetPtr);

      //get pubkey
      auto pubkey = assetSingle->getPubKey();
      auto& pubkeyData = pubkey->getUncompressedKey();

      return computeNextPublicEntry(pubkeyData,
         assetSingle->getAccountID(), assetSingle->getIndex() + 1);
   };

   vector<shared_ptr<AssetEntry>> assetVec;
   auto currentAsset = firstAsset;

   for (unsigned i = 0; i < count; i++)
   {
      currentAsset = nextAsset(currentAsset);
      assetVec.push_back(currentAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<AssetEntry_Single> 
   DerivationScheme_ArmoryLegacy::computeNextPrivateEntry(
   shared_ptr<DecryptedDataContainer> ddc,
   const SecureBinaryData& privKeyData, unique_ptr<Cypher> cypher,
   const BinaryData& accountID, unsigned index)
{
   //chain the private key
   auto&& nextPrivkeySBD = CryptoECDSA().ComputeChainedPrivateKey(
      privKeyData, chainCode_);

   //compute its pubkey
   auto&& nextPubkey = CryptoECDSA().ComputePublicKey(nextPrivkeySBD);

   //encrypt the new privkey
   auto&& newCypher = cypher->getCopy(); //copying a cypher cycles the IV
   auto&& encryptedNextPrivKey = ddc->encryptData(
      newCypher.get(), nextPrivkeySBD);

   //clear the unencrypted privkey object
   nextPrivkeySBD.clear();

   //instantiate new encrypted key object
   auto nextPrivKey = make_shared<Asset_PrivateKey>(
      index, encryptedNextPrivKey, move(newCypher));

   //instantiate and return new asset entry
   return make_shared<AssetEntry_Single>(
      index, accountID, nextPubkey, nextPrivKey);
}

////////////////////////////////////////////////////////////////////////////////
vector<shared_ptr<AssetEntry>> 
   DerivationScheme_ArmoryLegacy::extendPrivateChain(
   shared_ptr<DecryptedDataContainer> ddc,
   shared_ptr<AssetEntry> firstAsset, unsigned count)
{
   //throws is the wallet is locked or the asset is missing its private key

   auto nextAsset = [this, ddc](
      shared_ptr<AssetEntry> assetPtr)->shared_ptr<AssetEntry>
   {
      //sanity checks
      auto assetSingle =
         dynamic_pointer_cast<AssetEntry_Single>(assetPtr);

      auto privkey = assetSingle->getPrivKey();
      if (privkey == nullptr)
         throw AssetUnavailableException();
      auto& privkeyData =
         ddc->getDecryptedPrivateKey(privkey);

      auto id_int = assetSingle->getIndex() + 1;
      auto& account_id = assetSingle->getAccountID();

      return computeNextPrivateEntry(
         ddc, 
         privkeyData, move(privkey->copyCypher()),
         account_id, id_int);
   };

   if (ddc == nullptr)
      throw AssetUnavailableException();

   ReentrantLock lock(ddc.get());

   vector<shared_ptr<AssetEntry>> assetVec;
   auto currentAsset = firstAsset;

   for (unsigned i = 0; i < count; i++)
   {
      currentAsset = nextAsset(currentAsset);
      assetVec.push_back(currentAsset);
   }

   return assetVec;
}

////////////////////////////////////////////////////////////////////////////////
BinaryData DerivationScheme_ArmoryLegacy::serialize() const
{
   BinaryWriter bw;
   bw.put_uint8_t(DERIVATIONSCHEME_LEGACY);
   bw.put_var_int(chainCode_.getSize());
   bw.put_BinaryData(chainCode_);

   BinaryWriter final;
   final.put_var_int(bw.getSize());
   final.put_BinaryData(bw.getData());

   return final.getData();
}
