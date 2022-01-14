/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <gtest/gtest.h>
#include <google/protobuf/util/json_util.h>
#include "BtcUtils.h"
#include "bs_communication.pb.h"
//#include "AutheIDClient.h"

namespace {

   BinaryData mangleBegin(const BinaryData &d)
   {
      BinaryData result = d;
      result[0] = ~result[0];
      return result;
   }

   BinaryData mangleEnd(const BinaryData &d)
   {
      BinaryData result = d;
      result[int(result.getSize()) - 1] = ~result[int(result.getSize()) - 1];
      return result;
   }

}

#if 0
TEST(TestAuthEid, VerifySignature)
{
   auto testResult = R"(
      {
       "requestType": "ConfirmAuthAddressSubmitType",
       "requestData": "Chh0ZXN0MjFAdmVyaWZpZWQtZmFzdC5jb20SKnRiMXE2a3Q4czJuOWo1YWNoam1zYTZ6d2RnN255ODMwempneGd0dWg0eRgCIAMyQDQwZTdmMzBkYWZiMTQ4ZDE3ZTNhZmU2ZmViMGUxN2Y0ODI2YzZhYmJlYTE3YTk4ZWJjOTAyYWRhMjM3YjA2NGY=",
       "authEidSign": {
        "serialization": "PROTOBUF",
        "signature_data": "Chh0ZXN0MjFAdmVyaWZpZWQtZmFzdC5jb20SC0Jsb2NrU2V0dGxlGhZBdXRoZW50aWNhdGlvbiBBZGRyZXNzIiRTdWJtaXQgYXV0aCBhZGRyZXNzIGZvciB2ZXJpZmljYXRpb24ox/7d6AUwHkIglpIXElQukwKiqBsAl9/bUnzGuLVQP78lZjzIHhHL4PZIxv7d6AU=",
        "sign": "MEUCIC3OaokJrFkSZwhPEyxY+XhipZVu8UlC2DnJuw6nYc2bAiEAsnlQlhR6RtwJTP7lfPdR0rvMr/gnJzBzwY3N2uolbm0=",
        "certificate_client": "MIICQjCCAcegAwIBAgIRAOCz5zcLLR+Z0JsYTeVT8U8wCgYIKoZIzj0EAwMwfDELMAkGA1UEBhMCU0UxITAfBgNVBAoTGFRlc3QgQXV0aGVudGljYXRlIGVJRCBBQjEfMB0GA1UECxMWVGVzdCBJbmZyYXN0cnVjdHVyZSBDQTEpMCcGA1UEAxMgVGVzdCBBdXRoIGVJRCBJbnRlcm1lZGlhdGUgQ0EgdjEwIhgPMjAxOTA2MjkwODI2MzRaGA8yMDIwMDEwMTIzNTk1OVowQjERMA8GA1UEAxMIMnBxNTR0ZnIxETAPBgNVBAQTCFNwZWNpbWVuMQ0wCwYDVQQqEwRUZXN0MQswCQYDVQQGEwJTRTBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABEThKSrJ8P9L3oyo4iNbsP8S949av10+nyv0GoEpB8WWFNKrHTHpxcTjuwaQXW/I0AFGxTnf/SwSdBjVRCO3o7ujYDBeMB0GA1UdDgQWBBRL1O5WEcx4iQnslEPZZYOJPYdB+DAOBgNVHQ8BAf8EBAMCB4AwDAYDVR0TAQH/BAIwADAfBgNVHSMEGDAWgBSWx0IgupFEwrNVRTF/Bb7FAOsLyzAKBggqhkjOPQQDAwNpADBmAjEA5cEKfbqE9hD0XknUwkAdWh0jLMTZJ4uFlpiq8pkDuNufEOBCluoaZaHeirYmhdU+AjEAmv3eIarzO/6DVZC8NRsGmYjdCLMkoqS0D1PXoRadiAATzVgCB0H9Mhg+Kub+w8s8",
        "certificate_issuer": "MIICmjCCAiCgAwIBAgIVANEEthjspzxjiL+gyxL90aqZPT8uMAoGCCqGSM49BAMDMHQxCzAJBgNVBAYTAlNFMSEwHwYDVQQKExhUZXN0IEF1dGhlbnRpY2F0ZSBlSUQgQUIxHzAdBgNVBAsTFlRlc3QgSW5mcmFzdHJ1Y3R1cmUgQ0ExITAfBgNVBAMTGFRlc3QgQXV0aCBlSUQgUm9vdCBDQSB2MTAiGA8yMDE5MDUyMjAwMDAwMFoYDzIwMzkwNTIxMDAwMDAwWjB8MQswCQYDVQQGEwJTRTEhMB8GA1UEChMYVGVzdCBBdXRoZW50aWNhdGUgZUlEIEFCMR8wHQYDVQQLExZUZXN0IEluZnJhc3RydWN0dXJlIENBMSkwJwYDVQQDEyBUZXN0IEF1dGggZUlEIEludGVybWVkaWF0ZSBDQSB2MTB2MBAGByqGSM49AgEGBSuBBAAiA2IABOFr1w3OoSja0wvztw9T9SPXGZB9KVD+5CrUEJSsDj4zxCp9qCJ6DkZEUwaGvIpOWnAA2WAWrZhOTNJyMsq/sgQJHpb/yQUqrvRF/tEM2iy3cpMixYqi00eyFwqEWv1L8aNmMGQwHQYDVR0OBBYEFJbHQiC6kUTCs1VFMX8FvsUA6wvLMA4GA1UdDwEB/wQEAwIBBjASBgNVHRMBAf8ECDAGAQH/AgEAMB8GA1UdIwQYMBaAFFpaIwjHjLPx0FWwFXJHSov3BVJ6MAoGCCqGSM49BAMDA2gAMGUCMQDWJiSC0b63097Rs4Y1RNVsKVEeQXWo4gngmzsWofEQaqvc4OCSdDvck2etcXmPDewCMDn05jp3fd0srkL470ud7CCXTsw+bIBdQn3be8J+TSonIaJROj1JKAry4drvgSdR+w==",
        "ocsp_response": "MIIBnwoBAKCCAZgwggGUBgkrBgEFBQcwAQEEggGFMIIBgTCCAQehfjB8MQswCQYDVQQGEwJTRTEhMB8GA1UEChMYVGVzdCBBdXRoZW50aWNhdGUgZUlEIEFCMR8wHQYDVQQLExZUZXN0IEluZnJhc3RydWN0dXJlIENBMSkwJwYDVQQDEyBUZXN0IEF1dGggZUlEIEludGVybWVkaWF0ZSBDQSB2MRgPMjAxOTA2MjkxMTU3MDFaMHQwcjBKMAkGBSsOAwIaBQAEFNXndzsQYpoHnQaluQ3vhHlPLkCcBBT0Eo36lU2nSv2AiVCJs71uyUWrwAIRAOCz5zcLLR+Z0JsYTeVT8U+AABgPMjAxOTA2MjkxMTU3MDFaoBEYDzIwMTkwNjMwMTExMzEyWjAKBggqhkjOPQQDAwNoADBlAjBSGr1K3y5m3qmkABrPDfcbsr8nd/IIOEytKtfqHKHLxpyNs+ihXVX/Uh9k2yXO21YCMQDr0XmGwtbAKInn+GpozPOhzMvff4lddtGe4f2zYoVW3tx6tOyS4EOVeHwWmvmVym8="
       }
      }
   )";

   Blocksettle::Communication::RequestPacket packet;
   auto status = google::protobuf::util::JsonStringToMessage(testResult, &packet);
   ASSERT_TRUE(status.ok());

   AutheIDClient::SignResult signResult;
   signResult.serialization = AutheIDClient::Serialization(packet.autheidsign().serialization());
   signResult.data = BinaryData::fromString(packet.autheidsign().signature_data());
   signResult.sign = BinaryData::fromString(packet.autheidsign().sign());
   signResult.certificateClient = BinaryData::fromString(packet.autheidsign().certificate_client());
   signResult.certificateIssuer = BinaryData::fromString(packet.autheidsign().certificate_issuer());
   signResult.ocspResponse = BinaryData::fromString(packet.autheidsign().ocsp_response());

   auto result = AutheIDClient::verifySignature(signResult, AuthEidEnv::Staging);
   ASSERT_TRUE(result.valid);

   ASSERT_EQ(result.uniqueUserId, "2pq54tfr");

   auto requestDataHash = BtcUtils::getSha256(BinaryData::fromString(packet.requestdata()));
   ASSERT_EQ(result.invisibleData, requestDataHash);

   ASSERT_EQ(result.email, "test21@verified-fast.com");
   ASSERT_EQ(result.finished, std::chrono::system_clock::from_time_t(1561820999));

   ASSERT_TRUE(!result.rpName.empty());
   ASSERT_TRUE(!result.title.empty());
   ASSERT_TRUE(!result.description.empty());

   auto signResultInvalid = signResult;
   signResultInvalid.sign = mangleEnd(signResult.sign);
   result = AutheIDClient::verifySignature(signResultInvalid, AuthEidEnv::Staging);
   EXPECT_TRUE(!result.valid);

   signResultInvalid = signResult;
   signResultInvalid.certificateClient = mangleEnd(signResult.certificateClient);
   result = AutheIDClient::verifySignature(signResultInvalid, AuthEidEnv::Staging);
   EXPECT_TRUE(!result.valid);

   signResultInvalid = signResult;
   signResultInvalid.certificateIssuer = mangleEnd(signResult.certificateIssuer);
   result = AutheIDClient::verifySignature(signResultInvalid, AuthEidEnv::Staging);
   EXPECT_TRUE(!result.valid);

   signResultInvalid = signResult;
   signResultInvalid.ocspResponse = mangleBegin(signResult.ocspResponse);
   result = AutheIDClient::verifySignature(signResultInvalid, AuthEidEnv::Staging);
   EXPECT_TRUE(!result.valid);

   signResultInvalid = signResult;
   // Invalid check using production root CA
   result = AutheIDClient::verifySignature(signResultInvalid, AuthEidEnv::Prod);
   EXPECT_TRUE(!result.valid);
}
#endif   //0
