////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2019, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "ScriptRecipient.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////
//
// ScriptRecipient
//
////////////////////////////////////////////////////////////////////////////////
ScriptRecipient::~ScriptRecipient()
{}

////////////////////////////////////////////////////////////////////////////////
shared_ptr<ScriptRecipient> ScriptRecipient::deserialize(
   const BinaryDataRef& dataPtr)
{
   shared_ptr<ScriptRecipient> result_ptr;

   BinaryRefReader brr(dataPtr);

   auto value = brr.get_uint64_t();
   auto script = brr.get_BinaryDataRef(brr.getSizeRemaining());

   BinaryRefReader brr_script(script);

   auto byte0 = brr_script.get_uint8_t();
   auto byte1 = brr_script.get_uint8_t();
   auto byte2 = brr_script.get_uint8_t();

   if (byte0 == 25 && byte1 == OP_DUP && byte2 == OP_HASH160)
   {
      auto byte3 = brr_script.get_uint8_t();
      if (byte3 == 20)
      {
         auto&& hash160 = brr_script.get_BinaryData(20);
         result_ptr = make_shared<Recipient_P2PKH>(hash160, value);
      }
   }
   else if (byte0 == 22 && byte1 == 0 && byte2 == 20)
   {
      auto&& hash160 = brr_script.get_BinaryData(20);
      result_ptr = make_shared<Recipient_P2WPKH>(hash160, value);
   }
   else if (byte0 == 23 && byte1 == OP_HASH160 && byte2 == 20)
   {
      auto&& hash160 = brr_script.get_BinaryData(20);
      result_ptr = make_shared<Recipient_P2SH>(hash160, value);
   }
   else if (byte0 == 34 && byte1 == 0 && byte2 == 32)
   {
      auto&& hash256 = brr_script.get_BinaryData(32);
      result_ptr = make_shared<Recipient_P2WSH>(hash256, value);
   }
   else
   {
      //is this an OP_RETURN?
      if (byte0 == script.getSize() - 1 && byte1 == OP_RETURN)
      {
         if (byte2 == OP_PUSHDATA1)
            byte2 = brr_script.get_uint8_t();

         auto&& opReturnMessage = brr_script.get_BinaryData(byte2);
         result_ptr = make_shared<Recipient_OPRETURN>(opReturnMessage);
      }
   }

   if (result_ptr == nullptr)
      throw runtime_error("unexpected recipient script");

   return result_ptr;
}

////////////////////////////////////////////////////////////////////////////////
//
// Recipient_P2PKH
//
////////////////////////////////////////////////////////////////////////////////
void Recipient_P2PKH::serialize()
{
   BinaryWriter bw;
   bw.put_uint64_t(value_);

   auto&& rawScript = BtcUtils::getP2PKHScript(h160_);
   bw.put_var_int(rawScript.getSize());
   bw.put_BinaryData(rawScript);

   script_ = std::move(bw.getData());
}

////////////////////////////////////////////////////////////////////////////////
size_t Recipient_P2PKH::getSize() const 
{ 
   return 34; 
}

////////////////////////////////////////////////////////////////////////////////
//
// Recipient_P2PK
//
////////////////////////////////////////////////////////////////////////////////
void Recipient_P2PK::serialize()
{
   BinaryWriter bw;
   bw.put_uint64_t(value_);

   auto&& rawScript = BtcUtils::getP2PKScript(pubkey_);

   bw.put_var_int(rawScript.getSize());
   bw.put_BinaryData(rawScript);

   script_ = std::move(bw.getData());
}

////////////////////////////////////////////////////////////////////////////////
size_t Recipient_P2PK::getSize() const
{
   return 10 + pubkey_.getSize();
}

////////////////////////////////////////////////////////////////////////////////
//
// Recipient_P2WPKH
//
////////////////////////////////////////////////////////////////////////////////
void Recipient_P2WPKH::serialize()
{
   BinaryWriter bw;
   bw.put_uint64_t(value_);

   auto&& rawScript = BtcUtils::getP2WPKHOutputScript(h160_);

   bw.put_var_int(rawScript.getSize());
   bw.put_BinaryData(rawScript);

   script_ = std::move(bw.getData());
}

////////////////////////////////////////////////////////////////////////////////
size_t Recipient_P2WPKH::getSize() const
{ 
   return 31; 
}

////////////////////////////////////////////////////////////////////////////////
//
// Recipient_P2SH
//
////////////////////////////////////////////////////////////////////////////////
void Recipient_P2SH::serialize()
{
   BinaryWriter bw;
   bw.put_uint64_t(value_);

   auto&& rawScript = BtcUtils::getP2SHScript(h160_);

   bw.put_var_int(rawScript.getSize());
   bw.put_BinaryData(rawScript);

   script_ = std::move(bw.getData());
}

////////////////////////////////////////////////////////////////////////////////
size_t Recipient_P2SH::getSize() const
{
   return 32;
}

////////////////////////////////////////////////////////////////////////////////
//
// Recipient_P2WSH
//
////////////////////////////////////////////////////////////////////////////////
void Recipient_P2WSH::serialize()
{
   BinaryWriter bw;
   bw.put_uint64_t(value_);

   auto&& rawScript = BtcUtils::getP2WSHOutputScript(h256_);

   bw.put_var_int(rawScript.getSize());
   bw.put_BinaryData(rawScript);

   script_ = std::move(bw.getData());
}

////////////////////////////////////////////////////////////////////////////////
size_t Recipient_P2WSH::getSize() const
{
   return 43;
}

////////////////////////////////////////////////////////////////////////////////
//
// Recipient_OPRETURN
//
////////////////////////////////////////////////////////////////////////////////
void Recipient_OPRETURN::serialize()
{
   BinaryWriter bw;
   bw.put_uint64_t(0);

   BinaryWriter bw_msg;
   auto size = message_.getSize();
   if (size > 75)
   {
      bw_msg.put_uint8_t(OP_PUSHDATA1);
      bw_msg.put_uint8_t(size);
   }
   else if (size > 0)
   {
      bw_msg.put_uint8_t(size);
   }

   if (size > 0)
      bw_msg.put_BinaryData(message_);

   bw.put_uint8_t(bw_msg.getSize() + 1);
   bw.put_uint8_t(OP_RETURN);
   bw.put_BinaryData(bw_msg.getData());

   script_ = bw.getData();
}

////////////////////////////////////////////////////////////////////////////////
size_t Recipient_OPRETURN::getSize() const
{
   auto size = message_.getSize();
   if (size > 75)
      size += 2;
   else if (size > 0)
      size += 1;

   size += 9; //8 for value, one for op_return
   return size;
}

////////////////////////////////////////////////////////////////////////////////
//
// Recipient_Universal
//
////////////////////////////////////////////////////////////////////////////////
void Recipient_Universal::serialize()
{
   if (script_.getSize() != 0)
      return;

   BinaryWriter bw;
   bw.put_uint64_t(value_);
   bw.put_var_int(binScript_.getSize());
   bw.put_BinaryData(binScript_);

   script_ = std::move(bw.getData());
}

////////////////////////////////////////////////////////////////////////////////
size_t Recipient_Universal::getSize() const
{
   size_t varint_len = 1;
   if (binScript_.getSize() >= 0xfd)
      varint_len = 3; //larger scripts would make the tx invalid

   return 8 + binScript_.getSize() + varint_len;
}