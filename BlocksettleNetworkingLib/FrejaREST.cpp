#include "FrejaREST.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QThread>
#include <spdlog/spdlog.h>

static const QByteArray caCertData = "-----BEGIN CERTIFICATE-----\n"
   "MIIGMzCCBBugAwIBAgIUPB4rUqFFiG6g67a+cLCBCuTkxGQwDQYJKoZIhvcNAQEL\n"
   "BQAwUTELMAkGA1UEBhMCU0UxEzARBgNVBAoTClZlcmlzZWMgQUIxEjAQBgNVBAsT\n"
   "CUZyZWphIGVJRDEZMBcGA1UEAxMQUlNBIFRlc3QgUm9vdCBDQTAeFw0xNzA1MTAx\n"
   "NDI2MDBaFw00NzA1MTAxNDI2MDBaMFExCzAJBgNVBAYTAlNFMRMwEQYDVQQKEwpW\n"
   "ZXJpc2VjIEFCMRIwEAYDVQQLEwlGcmVqYSBlSUQxGTAXBgNVBAMTEFJTQSBUZXN0\n"
   "IFJvb3QgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQC6n6IvJcOI\n"
   "y9y4x4YZlcDYWGANZn/58aQq/+q/2IOheqH7pfqf00FrZmTFzXQTI4koPUOpagYM\n"
   "ESG6MLlgW7akCnA3V5duEvGBJgAR6FldaiwdHMqWBKLb5pvoC2/uczSNie+pEidQ\n"
   "uj+Oh5MwUCJWx4n2fLoJMTP4Lb1nxFQXzCjRMWJ1w3pM+3mDYJzvLFhV2Ur7QBAd\n"
   "JjGGPCprDdREfzanm7Jg5mFtdtbMPPobMVDKRiCvfXLavE4UeupJF2Rdg530tpaJ\n"
   "Mb6m++OsFMN4sHq0HUYiYIwetdmxY3W2dpKJjmL7pPPprcpnHqci9a3N32ajclpV\n"
   "Z7c0jfuwCwk+6EFYRNmCkKEkMrSe8wr8tuH4FYwhTQCsFQeAWUaWzSl29Ielmx38\n"
   "Ot+g3aUw8LZltZzMYhak257bx4Lqfr23edjz2g45/DEk5H2/zsvEGnwq73xtpAJZ\n"
   "rZHSqgugwPqLhCxKs93abuShMas92CL7juAp4FjYzjBS85qQnHhxVFziGoyvtUU3\n"
   "YS6ZNae96KbgW7Kjd72i/wfUNJKdF2QAKWIJYL80bQ9m2w+sL6TNd/tRG3OXWJHD\n"
   "prKRTYKiW2nZxDoX4ClsNMWj2iKPaGtbl6tmZpRLZtjs8s9lAiNBQd0XqtTsyyr/\n"
   "3+8Afnhs+DG55A4/91DdaXlDA4UbpjZpDQIDAQABo4IBATCB/jAOBgNVHQ8BAf8E\n"
   "BAMCAQYwEgYDVR0TAQH/BAgwBgEB/wIBAzCBuAYDVR0gBIGwMIGtMIGqBgUqAwQF\n"
   "BjCBoDA4BggrBgEFBQcCARYsaHR0cHM6Ly9jcHMudGVzdC5mcmVqYWVpZC5jb20v\n"
   "Y3BzL2luZGV4Lmh0bWwwZAYIKwYBBQUHAgIwWAxWVGhpcyBjZXJ0aWZpY2F0ZSBo\n"
   "YXMgYmVlbiBpc3N1ZWQgIGluIGFjY29yZGFuY2Ugd2l0aCB0aGUgRnJlamEgZUlE\n"
   "IFRFU1QgUG9saWN5IENvbnRyb2wwHQYDVR0OBBYEFMqw7LlXw5w9rTmKE/rwNbrs\n"
   "DHeKMA0GCSqGSIb3DQEBCwUAA4ICAQBOGI2Y4uXQeAMSswESsIsbF4RlkvIQiGCd\n"
   "kwt7OzpfiRcOQnkxm9rlpdPtC7MajVI6owtZwT6BSG0jmyUFLihp4VB02VM02xkc\n"
   "YsSD/58V+Gf/1iEjgQgnNjz9Z5bURGUiPK9TWrchi7E2MLlySeHAEJUU1u5hwU0V\n"
   "+0hQ4S+EEZBYfOV5WaoFma2YXFTSSCHtzmG+OMhItgevJFt+OLymOTewuF7v4vcP\n"
   "PVyUB9iEgawEwpjJEBtaxkmIaJv4J/c92KKHcTKxr8EaPfOl4t3UCHmQLgnCEG/3\n"
   "Hn6KgNsH6RCOmZojdTf5vwQZ2B7AcbVozU/noJZ1o6C4oRt5PkTEdSnAmX8pf4Mn\n"
   "NXYmxPpXE7KlEazLx9poBGVobCn0X3F+1A5pEHfY8Oy/EOKc3+ZswW294AuWCs/n\n"
   "HlamWPS+jqNKW3qjjNK6FZs72IECuf9OSN5BvDrUsW44b0Y6oGIUevOtexAXiBWV\n"
   "SKT9GsojrlY36X0O3+lkkqtW4aea11qi3oGz+9iXcPQeeD7kgfkszSYKkn9WB1YT\n"
   "j/lpZTlf9DlxA5++uu3Grpx7qRdClEbDf5Q2HLISWVwirocySGzh4wACFHi6iQjn\n"
   "srnzHu968MtOnN6FQt9zPZxaRYrzLpV/9yyah9jYYuLFIGje+yzAn5M8ORV5p1At\n"
   "FvjTRfH5oA==\n"
   "-----END CERTIFICATE-----\n";

static const QByteArray certData = "-----BEGIN CERTIFICATE-----\n"
   "MIIEGjCCAwKgAwIBAgIUfqHT8k5Hi6Ihc6Zxpuo9ww5NsqowDQYJKoZIhvcNAQEL\n"
   "BQAwgYMxCzAJBgNVBAYTAlNFMRIwEAYDVQQHEwlTdG9ja2hvbG0xFDASBgNVBGET\n"
   "CzU1OTExMC00ODA2MR0wGwYDVQQKExRWZXJpc2VjIEZyZWphIGVJRCBBQjENMAsG\n"
   "A1UECxMEVGVzdDEcMBoGA1UEAxMTUlNBIFRFU1QgSXNzdWluZyBDQTAeFw0xODA2\n"
   "MTgxNDQ2NTZaFw0yMTA2MTgxNDQ2NTZaMGoxCzAJBgNVBAYTAlNFMRQwEgYDVQRh\n"
   "Ews1NTkwNTctMDE5NzEXMBUGA1UEChMOQmxvY2tzZXR0bGUgQUIxDTALBgNVBAsT\n"
   "BFRlc3QxHTAbBgNVBAMTFEJsb2Nrc2V0dGxlIFNlcnZpY2VzMIIBIjANBgkqhkiG\n"
   "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAj3cwpzPNpjgvnnyVViOgEoafg7RVFSZY2DTD\n"
   "Xl0aKBMDjV75JHX7tNUbMQ3Ja4cYRwBBYVQk4SOCVHfU93zLuJnuoq4Z+E4eqtrf\n"
   "lshJ2gt3RWyrmwPrt54N2R9SJ9afjWczSDPRf+EE5ksDynKUPuS/2abQ6nMaRfxG\n"
   "LEw0I7gEgtN/bvOA0ae6MKmMIkPEoQH/xv8/BllQPAuBTsthJ6key6H4qCDaX1ff\n"
   "7f+PYjjyGZ4pUSujs7nkPSseSLPQoAiL/myBTpwfOo3EUyqkwXCKNg96CIdJPDBf\n"
   "i7BVVAiUpg3vnLtFro2NLTKybHBalBpEqZ4pHAsbEwlsrDWMNQIDAQABo4GdMIGa\n"
   "MA4GA1UdDwEB/wQEAwIF4DAMBgNVHRMBAf8EAjAAMBEGCWCGSAGG+EIBAQQEAwIH\n"
   "gDAfBgNVHSMEGDAWgBRqfIoPnXAOHNpfLaA8Jl+I6BW/nDASBgNVHSAECzAJMAcG\n"
   "BSoDBAUHMB0GA1UdDgQWBBT2ucygZzzlNlsmt+YViDjLDMbShTATBgNVHSUEDDAK\n"
   "BggrBgEFBQcDAjANBgkqhkiG9w0BAQsFAAOCAQEAcb+LHp5pJ+wEPmCX3ZSATU1m\n"
   "IsILJSxNsBGCl5GzgdpGGMhyUu0Q7BVQwJ131vNRZ8BcXDsTKHC0xuhZfFEnXSHd\n"
   "7XHf1Hwqk+yKyY6uokL+DYtqpAVK6tWFw1clnZVn5YsYZdD0usr0nctTs2AOJ71h\n"
   "nMHNLCHSa+5f+tvWLNxP1rGACGM9pvUOrrv5kfcCtZb2TYT2hqCj8184bRx1wfXR\n"
   "D8QOG5YIWToDy1iug7J3hdKrIVNyJiG2Co87GlHwylj1+kxQ9f2/e/WJfvJPb3B3\n"
   "ALjhFpQ/Ka74i0toqsEhbWv0B+QrMZk2Y5ILUTszGDRzi+Dq59g2rUyjHUV1Qw==\n"
   "-----END CERTIFICATE-----\n";

static const QByteArray keyData = "-----BEGIN ENCRYPTED PRIVATE KEY-----\n"
   "MIIFDjBABgkqhkiG9w0BBQ0wMzAbBgkqhkiG9w0BBQwwDgQIOL+bpzx2KDwCAggA\n"
   "MBQGCCqGSIb3DQMHBAhYIzg1PldXVQSCBMio7QTZMJawtiLUndVvXRVZV2cqAJsr\n"
   "Movgcii4bJ0jAM7gebCQrmERq/iyjz7pPFL3HBNwonovGiL1Y5Nlhr3L2d0u/z9O\n"
   "zt0UplNeJOeMZ9E2J2aKLJPFKG+jClAeaimj2MHOIb3dSFbvzyRh8c3IkmQmCAYR\n"
   "ddkN69snsCCbfL3MBlMO7X3lovyL0vjdFHjK1zf9ZaqtvAsYDPAD3pYqjg17J/ef\n"
   "DNKn1L0p3evgI9/vW/ocsUazPh7A2nfYgZ/YzuCwYY5w5vjcPamnlARRT0uR4Msm\n"
   "Pgha7DPGgW1n8/i2fzKVBwN39w9RA5QaSGpQXCblYQPmco6kMeIAruCLvJklHZ/F\n"
   "zEKjaNpXd9i4PyFCmD5KCUpvYZiauUIVCTqjNlsYfYtZAhbNx+2iVLt+5mNDBuyW\n"
   "ILdYZ40fEfFUV/n8r/Vw2ahYVDlGqo19UYNm1WdSQQXmZcY+h4lo48OltwhXtjPM\n"
   "Z/db1r6JbMhNbFqR2WBXoLTtKskv3g/CXiO7GDmJ91aPXDDaMLmqN68WMN6S6eyR\n"
   "h/oKgaQri/G2vNPH7gXWBRDlAhmREVdIonQkkieIdSWjp+0Qr7QuyvhxnTtBgJHU\n"
   "6ruQWu8ZBQhEzBOhxXWb3vPjnINsuogBkcw91si5A8p01gN6kWVhxzxMheBPPpUy\n"
   "5NE2/KCbv3t6oYqNV55ejPiWSznR4vkYB3TqK7EJZqAJ6b3I5wj19g7xuCAsl01Z\n"
   "gmYUTV2vJdfX0rd7nyCFZmxQL/nFZikMhyuDCI8UqRunnCC/mjSE+SefOpPAcroU\n"
   "C7XgA2gG21+9ur2Uwsx1nPxT3HeaJUALMGwnE0zPhxubQgSD5hV5YM+KWixM+kky\n"
   "8zIZPY8+Z1UEhTbiX0Kc8tJgt/zPkI5aKJfed6rsU10bGUgTlroqrLxWPhb2sYl6\n"
   "xsa7Ukv/15vdEhujyid2zOXaz4PKaz+f0P6F/SEq8Qr3mzQSDrNpDkAxjupvIFm6\n"
   "gkV2hqacWkJ6TCqmK1DRK/MTZKLd5W4wzGUaAV2g7VnyiV6WXuI20tPSdmd6eLNs\n"
   "9r3NPN7F4tmppO8ZJ2X03VxtKf/YgS/Z1e3c7Eq3YBalRcWQRpTLHbXpwSzlISOI\n"
   "hsY/rQGMv7dTGo+EYgST1cHcHNHrtwSI4xzz666E09yiiLyN39LiOQM/Nm9wecDM\n"
   "vnZp9+LS/JqiaDN8Fwgio++Qxx+ZUbdaDyJRgPCmqjymXnL70wXgGOnHWpNnAjNH\n"
   "CcgRxEwy/Y+xWhv1gftNqv8mLyBa09SOG8u7dh+B738EgZLDeZPFRvpvWjzG2mxA\n"
   "FdJGTEercd86Ccp74reZ6C8HUSJ4GtkEcppp07Awdbh9cZHIssXTiOdA90FWF9fh\n"
   "bixcFN6G5CT/izvRhyFZLk0CIy9lvT9sxcxN+W2kvNOMgWLy+3L+E9lqi+CYqUD7\n"
   "3cBoKNLjLaTdUEx8BCVEnTifHv/J97PYf/5NiHxEo45J2mVqs9Uw8qBwt0+nwvky\n"
   "R+86xbAMVApraJsI7VYNwwE1Pwdrp+ggAzGh7jj9E1cCGKk/irt5WgRlZ3r8uwyV\n"
   "jSR8wvu0lvpo/M89iH7fcqZCb3hiAgdsh+XyDfg6xwesXL1MOmjCF9IMkPii8hsD\n"
   "0cI=\n"
   "-----END ENCRYPTED PRIVATE KEY-----\n";

static const QByteArray signCertData = "-----BEGIN CERTIFICATE-----\n"
   "MIIEETCCAvmgAwIBAgIUTeCJ0hz3mbtyONBEiap7su74LZwwDQYJKoZIhvcNAQEL\n"
   "BQAwgYMxCzAJBgNVBAYTAlNFMRIwEAYDVQQHEwlTdG9ja2hvbG0xFDASBgNVBGET\n"
   "CzU1OTExMC00ODA2MR0wGwYDVQQKExRWZXJpc2VjIEZyZWphIGVJRCBBQjENMAsG\n"
   "A1UECxMEVGVzdDEcMBoGA1UEAxMTUlNBIFRFU1QgSXNzdWluZyBDQTAeFw0xNzA3\n"
   "MTIxNTIwMTNaFw0yMDA3MTIxNTIwMTNaMIGKMQswCQYDVQQGEwJTRTESMBAGA1UE\n"
   "BxMJU3RvY2tob2xtMRQwEgYDVQRhEws1NTkxMTAtNDgwNjEdMBsGA1UEChMUVmVy\n"
   "aXNlYyBGcmVqYSBlSUQgQUIxDTALBgNVBAsTBFRlc3QxIzAhBgNVBAMTGkZyZWph\n"
   "IGVJRCBURVNUIE9yZyBTaWduaW5nMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n"
   "CgKCAQEAgMINs87TiouDPSSmpn05kZv9TN8XdopcHnElp6ElJLpQh3oYGIL4B71o\n"
   "IgF3r8zRWq8kQoJlYMugmhsld0r0EsUJbsrcjBJ5CJ1WYZg1Vu8FpYLKoaFRI/qx\n"
   "T6xCMvd238Q99Sdl6G6O9sQQoFq10EaYBa970Tl3nDziQQ6bbSNkZoOYIZoicx4+\n"
   "1XFsrGiru8o8QIyc3g0eSgrd3esbUkuk0eH65SeaaOCrsaCOpJUqEziD+el4R6d4\n"
   "0dTz/uxWmNpGKF4BmsNWeQi9b4gDYuFqNYhs7bnahvkK6LvtDThV79395px/oUz5\n"
   "BEDdVwjxPJzgaAuUHE+6A1dMapkjsQIDAQABo3QwcjAOBgNVHQ8BAf8EBAMCBsAw\n"
   "DAYDVR0TAQH/BAIwADAfBgNVHSMEGDAWgBRqfIoPnXAOHNpfLaA8Jl+I6BW/nDAS\n"
   "BgNVHSAECzAJMAcGBSoDBAUKMB0GA1UdDgQWBBT7j90x8xG2Sg2p7dCiEpsq3mo5\n"
   "PTANBgkqhkiG9w0BAQsFAAOCAQEAaKEIpRJvhXcN3MvP7MIMzzuKh2O8kRVRQAoK\n"
   "Cj0K0R9tTUFS5Ang1fEGMxIfLBohOlRhXgKtqJuB33IKzjyA/1IBuRUg2bEyecBf\n"
   "45IohG+vn4fAHWTJcwVChHWcOUH+Uv1g7NX593nugv0fFdPqt0JCnsFx2c/r9oym\n"
   "+VPP7p04BbXzYUk+17qmFBP/yNlltjzfeVnIOk4HauR9i94FrfynuZLuItB6ySCV\n"
   "mOlfA0r1pHv5sofBEirhwceIw1EtFqEDstI+7XZMXgDwSRYFc1pTjrWMaua2Uktm\n"
   "JyWZPfIY69pi/z4u+uAnlPuQZnksaGdZiIcAyrt5IXpNCU5wyg==\n"
   "-----END CERTIFICATE-----\n";

static const QString sReqAuthInit = QLatin1String("initAuthentication");
static const QString sReqAuthResult = QLatin1String("getAuthResult");
static const QString sReqSignInit = QLatin1String("initSignature");
static const QString sReqSignResult = QLatin1String("getSignResult");
static const QString sReqSignCancel = QLatin1String("cancelSignature");

const auto sApproved = QLatin1String("APPROVED");
const auto sCancelled = QLatin1String("CANCELED");
const auto sRejected = QLatin1String("REJECTED");
const auto sExpired = QLatin1String("EXPIRED");


FrejaREST::FrejaREST(const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), logger_(logger)
   , caCertData_(caCertData), keyData_(keyData), certData_(certData)
   , keyPassphrase_("key-passphrase")
   , baseUrl_(QLatin1String("https://services.test.frejaeid.com/"))
   , baseAuthPath_(QLatin1String("authentication/1.0/"))
   , baseSignPath_(QLatin1String("sign/1.0/"))
   , seqNo_(1)
{
   processors_ = {
      { sReqAuthInit.toStdString()
      , [this](QNetworkReply *reply, const QJsonObject &response) -> bool {
         const auto &authRefIt = response.find(QLatin1String("authRef"));
         if (authRefIt == response.end()) {
            return false;
         }
         else {
            emit repliedInitAuthRequest(reply->property("SeqNo").toUInt(), authRefIt.value().toString());
         }
         return true;
      } },

      { sReqAuthResult.toStdString()
         , [this](QNetworkReply *reply, const QJsonObject &response) -> bool {
         const auto &statusIt = response.find(QLatin1String("status"));
         if (statusIt == response.end()) {
            return false;
         }
         const auto &status = statusIt.value().toString();
         if (status == sApproved) {
            const auto &detailsIt = response.find(QLatin1String("details"));
            if (detailsIt == response.end()) {
               emit repliedAuthRequestStatus(reply->property("SeqNo").toUInt(), status,{});
            }
            else {
               emit repliedAuthRequestStatus(reply->property("SeqNo").toUInt(), status
                  , detailsIt.value().toString());
            }
         }
         else {
            emit repliedAuthRequestStatus(reply->property("SeqNo").toUInt(), status,{});
         }
         return true;
      } },

      { sReqSignInit.toStdString()
         , [this](QNetworkReply *reply, const QJsonObject &response) -> bool {
         const auto &signRefIt = response.find(QLatin1String("signRef"));
         if (signRefIt == response.end()) {
            return false;
         }
         else {
            emit repliedInitSignRequest(reply->property("SeqNo").toUInt(), signRefIt.value().toString());
         }
         return true;
      } },

      { sReqSignResult.toStdString()
         , [this](QNetworkReply *reply, const QJsonObject &response) -> bool {
         const auto &statusIt = response.find(QLatin1String("status"));
         if (statusIt == response.end()) {
            return false;
         }
         const auto &status = statusIt.value().toString();
         const auto seqNo = reply->property("SeqNo").toUInt();
         if (status == sApproved) {
            const auto &detailsIt = response.find(QLatin1String("details"));
            if (detailsIt != response.end()) {
               const auto &details = detailsIt.value().toString();
               const auto detailData = details.split(QLatin1Char('.'));
               const auto payload = QByteArray::fromBase64(detailData[1].toUtf8());
               QJsonParseError error;
               QJsonDocument docPayload = QJsonDocument::fromJson(payload, &error);
               if (error.error != QJsonParseError::NoError) {
                  logger_->error("[FrejaREST::SignResult] Failed to parse payload json: {}"
                     , error.errorString().toStdString());
                  return false;
               }
               const auto &objPayload = docPayload.object();
               const auto &sigData = objPayload[QLatin1String("signatureData")].toObject();
               const auto userSigIt = sigData.find(QLatin1String("userSignature"));
               if (userSigIt != sigData.end()) {
                  const auto sigData = userSigIt.value().toString().split(QLatin1Char('.'));
                  const auto signature = QByteArray::fromBase64(sigData[2].toUtf8(), QByteArray::Base64UrlEncoding);
                  emit repliedSignRequestStatus(seqNo, status, signature);
               }
               else {
                  logger_->warn("[FrejaREST::SignResult] no userSignature found");
                  emit repliedSignRequestStatus(seqNo, status, {});
               }
            }
            else {
               logger_->warn("[FrejaREST::SignResult] no details found");
               emit repliedSignRequestStatus(seqNo, status,{});
            }
         }
         else {
            emit repliedSignRequestStatus(seqNo, status,{});
         }
         return true;
      } },

      { sReqSignCancel.toStdString()
         , [this](QNetworkReply *reply, const QJsonObject &response) -> bool {
         emit repliedSignRequestStatus(reply->property("SeqNo").toUInt(), sCancelled, {});
         return true;
      } },
   };

   init();
   connect(&nam_, &QNetworkAccessManager::finished, this, &FrejaREST::requestFinished);
   connect(&nam_, &QNetworkAccessManager::sslErrors, this, &FrejaREST::onSslErrors);
}

void FrejaREST::initData(const QByteArray &caCertData, const QByteArray &keyData, const QByteArray &keyPassphrase
   , const QByteArray &certData)
{
   caCertData_ = caCertData;
   keyData_ = keyData;
   keyPassphrase_ = keyPassphrase;
   certData_ = certData;
   init();
}

void FrejaREST::init()
{
   if (!QSslSocket::supportsSsl()) {
      throw std::runtime_error("SSL not supported");
   }

   const auto caCerts = QSslCertificate::fromData(caCertData_);
   if (caCerts.isEmpty() || caCerts.at(0).isNull()) {
      throw std::runtime_error("failed to decode CA certificate[s]");
   }
   sslConf_.setCaCertificates(caCerts);

   const QSslCertificate cert(certData_);
   if (cert.isNull()) {
      throw std::runtime_error("failed to decode certificate");
   }
   sslConf_.setLocalCertificate(cert);

   const QSslKey key(keyData_, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, keyPassphrase_);
   if (key.isNull()) {
      throw std::runtime_error("failed to decode key");
   }
   sslConf_.setPrivateKey(key);

   sslConf_.setProtocol(QSsl::SecureProtocols);
}

FrejaREST::SeqNo FrejaREST::sendInitAuthRequest(const QString &email)
{
   QNetworkRequest reqAuthInit;
   reqAuthInit.setSslConfiguration(sslConf_);
   reqAuthInit.setUrl(QUrl{ baseUrl_.url() + baseAuthPath_ + sReqAuthInit });
   reqAuthInit.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));

   QJsonObject msg;
   msg[QLatin1String("userInfoType")] = QLatin1String("EMAIL");
   msg[QLatin1String("userInfo")] = email;
   msg[QLatin1String("minRegistrationLevel")] = QLatin1String("BASIC");

   const QJsonDocument reqDoc(msg);
   const auto reqStr = reqDoc.toJson(QJsonDocument::Compact);

   auto reply = nam_.post(reqAuthInit, QByteArray{ "initAuthRequest=" } + reqStr.toBase64());
   if (!reply) {
      return 0;
   }
   reply->setProperty("reqType", sReqAuthInit);
   reply->setProperty("email", email);
   SeqNo seqNo = seqNo_++;
   reply->setProperty("SeqNo", seqNo);
   return seqNo;
}

FrejaREST::SeqNo FrejaREST::requestAuthStatus(const QString &authRef)
{
   QNetworkRequest reqAuthResult;
   reqAuthResult.setSslConfiguration(sslConf_);
   reqAuthResult.setUrl(QUrl{ baseUrl_.url() + baseAuthPath_ + QLatin1String("getOneResult") });
   reqAuthResult.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));

   QJsonObject msg;
   msg[QLatin1String("authRef")] = authRef;
   const QJsonDocument reqDoc(msg);
   const auto reqStr = reqDoc.toJson(QJsonDocument::Compact);

   auto reply = nam_.post(reqAuthResult, QByteArray{ "getOneAuthResultRequest=" } + reqStr.toBase64());
   if (!reply) {
      return 0;
   }
   reply->setProperty("reqType", sReqAuthResult);
   reply->setProperty("authRef", authRef);
   SeqNo seqNo = seqNo_++;
   reply->setProperty("SeqNo", seqNo);
   return seqNo;
}

FrejaREST::SeqNo FrejaREST::sendSignRequest(const QString &email, const QString &title, const QString &data)
{
   QNetworkRequest reqSignInit;
   reqSignInit.setSslConfiguration(sslConf_);
   reqSignInit.setUrl(QUrl{ baseUrl_.url() + baseSignPath_ + sReqSignInit });
   reqSignInit.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));

   QJsonObject msg, dataObj;
   msg[QLatin1String("userInfoType")] = QLatin1String("EMAIL");
   msg[QLatin1String("userInfo")] = email;
   msg[QLatin1String("minRegistrationLevel")] = QLatin1String("BASIC");
   msg[QLatin1String("title")] = title;
   msg[QLatin1String("confidential")] = false;
   msg[QLatin1String("dataToSignType")] = QLatin1String("SIMPLE_UTF8_TEXT");
/*   dataObj[QLatin1String("text")] = QString::fromLatin1(data.toUtf8().toBase64());
   msg[QLatin1String("dataToSign")] = dataObj;*/
   msg[QLatin1String("dataToSign")] = QLatin1String("{\"text\":\"")  // ugly hack for Freja's JSON incompatibility
      + QString::fromLatin1(data.toUtf8().toBase64()) + QLatin1String("\"}");
/*   dataObj[QLatin1String("title")] = title;
   dataObj[QLatin1String("text")] = QLatin1String("BlockSettle push notification text");
   msg[QLatin1String("pushNotification")] = dataObj;*/
   msg[QLatin1String("signatureType")] = QLatin1String("SIMPLE");

   const QJsonDocument reqDoc(msg);
   const auto reqStr = reqDoc.toJson(QJsonDocument::Compact);

   auto reply = nam_.post(reqSignInit, QByteArray{ "initSignRequest=" } + reqStr.toBase64());
   if (!reply) {
      return 0;
   }
   reply->setProperty("reqType", sReqSignInit);
   reply->setProperty("email", email);
   reply->setProperty("title", title);
   SeqNo seqNo = seqNo_++;
   reply->setProperty("SeqNo", seqNo);
   return seqNo;
}

FrejaREST::SeqNo FrejaREST::requestSignStatus(const QString &signRef)
{
   QNetworkRequest reqAuthResult;
   reqAuthResult.setSslConfiguration(sslConf_);
   reqAuthResult.setUrl(QUrl{ baseUrl_.url() + baseSignPath_ + QLatin1String("getOneResult") });
   reqAuthResult.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));

   QJsonObject msg;
   msg[QLatin1String("signRef")] = signRef;
   const QJsonDocument reqDoc(msg);
   const auto reqStr = reqDoc.toJson(QJsonDocument::Compact);

   auto reply = nam_.post(reqAuthResult, QByteArray{ "getOneSignResultRequest=" } + reqStr.toBase64());
   if (!reply) {
      return 0;
   }
   reply->setProperty("reqType", sReqSignResult);
   reply->setProperty("signRef", signRef);
   SeqNo seqNo = seqNo_++;
   reply->setProperty("SeqNo", seqNo);
   return seqNo;
}

FrejaREST::SeqNo FrejaREST::cancelSignRequest(const QString &signRef)
{
   QNetworkRequest reqAuthResult;
   reqAuthResult.setSslConfiguration(sslConf_);
   reqAuthResult.setUrl(QUrl{ baseUrl_.url() + baseSignPath_ + QLatin1String("cancel") });
   reqAuthResult.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/json"));

   QJsonObject msg;
   msg[QLatin1String("signRef")] = signRef;
   const QJsonDocument reqDoc(msg);
   const auto reqStr = reqDoc.toJson(QJsonDocument::Compact);

   auto reply = nam_.post(reqAuthResult, QByteArray{ "cancelSignRequest=" } + reqStr.toBase64());
   if (!reply) {
      return 0;
   }
   reply->setProperty("reqType", sReqSignCancel);
   reply->setProperty("signRef", signRef);
   SeqNo seqNo = seqNo_++;
   reply->setProperty("SeqNo", seqNo);
   return seqNo;
}

void FrejaREST::requestFinished(QNetworkReply *reply)
{
   if (!reply) {
      emit requestFailed(0);
      return;
   }

   const auto seqNo = reply->property("SeqNo").toUInt();
   const auto message = reply->readAll();

   if (message.isEmpty()) {
      logger_->error("[FrejaREST::requestFinished] received error {}", (int)reply->error());
      emit requestFailed(seqNo);
      return;
   }
   else {
      logger_->debug("[FrejaREST::requestFinished] received reply: {}", message.toStdString());
      emit emptyReply();
   }

   const auto &reqType = reply->property("reqType").toString();
   if (reqType.isEmpty()) {
      logger_->error("[FrejaREST::requestFinished] no request type in reply");
      emit requestFailed(seqNo);
      return;
   }

   QJsonParseError error;
   const QJsonDocument doc = QJsonDocument::fromJson(message, &error);
   if (error.error != QJsonParseError::NoError) {
      logger_->error("[FrejaREST::requestFinished] failed to parse response: {}"
         , error.errorString().toStdString());
      emit requestFailed(seqNo);
      return;
   }
   const auto &response = doc.object();

   const auto &processorIt = processors_.find(reqType.toStdString());
   if (processorIt == processors_.end()) {
      logger_->error("[FrejaREST::requestFinished] unknown reply type: {}"
         , reqType.toStdString());
      emit requestFailed(seqNo);
      return;
   }
   if (!processorIt->second(reply, response)) {
      logger_->error("[FrejaREST::requestFinished] {} processing failed"
         , reqType.toStdString());
      emit requestFailed(seqNo);
   }
}

void FrejaREST::onSslErrors(QNetworkReply *reply, const QList<QSslError> &errors)
{
   QStringList sErrors;
   for (const auto &error : errors) {
      sErrors.push_back(error.errorString());
   }
   logger_->error("[FrejaREST::onSslErrors] SSL error[s]: {}"
      , sErrors.join(QLatin1String(", ")).toStdString());

   if (!reply) {
      emit requestFailed(0);
      return;
   }
   emit requestFailed(reply->property("SeqNo").toUInt());
}


FrejaAuth::FrejaAuth(const std::shared_ptr<spdlog::logger> &logger)
   : QObject(nullptr), logger_(logger), freja_(logger)
{
   timer_.setInterval(1000);
   connect(&timer_, &QTimer::timeout, this, &FrejaAuth::onTimer);

   connect(&freja_, &FrejaREST::repliedInitAuthRequest, this, &FrejaAuth::onRepliedInitAuthRequest);
   connect(&freja_, &FrejaREST::repliedAuthRequestStatus, this, &FrejaAuth::onRepliedAuthRequestStatus);
   connect(&freja_, &FrejaREST::requestFailed, this, &FrejaAuth::onRequestFailed);
}

bool FrejaAuth::start(const QString &userId)
{
   if (userId.isEmpty()) {
      return false;
   }
   stopped_ = false;
   authRef_.clear();
   const auto reqId_ = freja_.sendInitAuthRequest(userId);
   if (!reqId_) {
      return false;
   }
   userId_ = userId;
   emit statusUpdated(userId_, tr("request sent"));
   logger_->debug("Freja auth started for {}", userId.toStdString());
   return true;
}

void FrejaAuth::stop()
{
   timer_.stop();
   stopped_ = true;
   authRef_.clear();
   logger_->debug("Freja auth stopped for {}", userId_.toStdString());
}

void FrejaAuth::onTimer()
{
   if (stopped_ || authRef_.isEmpty() || reqId_) {
      return;
   }
   const auto reqId_ = freja_.requestAuthStatus(authRef_);
   if (!reqId_) {
      logger_->error("[FrejaAuth] failed to request status");
   }
}

void FrejaAuth::onRepliedInitAuthRequest(FrejaREST::SeqNo, const QString &authRef)
{
   authRef_ = authRef;
   reqId_ = 0;
   timer_.start();
}

void FrejaAuth::onRepliedAuthRequestStatus(FrejaREST::SeqNo, const QString &status, const QString &details)
{
   if (stopped_) {
      return;
   }
   reqId_ = 0;
   if (status == sApproved) {
      stop();
      emit succeeded(userId_, details);
   }
   else if (status == sCancelled) {
      stop();
      emit failed(userId_, tr("cancelled by user"));
   }
   else if (status == sRejected) {
      stop();
      emit failed(userId_, tr("rejected"));
   }
   else if (status == sExpired) {
      stop();
      emit failed(userId_, tr("timed out"));
   }
   else if (status_ != status) {
      emit statusUpdated(userId_, status);
   }
   status_ = status;
}

void FrejaAuth::onRequestFailed(FrejaREST::SeqNo)
{
   stop();
   emit failed(userId_, tr("request failed"));
}


Q_DECLARE_METATYPE(SecureBinaryData)

FrejaSign::FrejaSign(const std::shared_ptr<spdlog::logger> &logger, unsigned int pollInterval)
   : QObject(nullptr), logger_(logger), freja_(logger), waitForReply_(false)
{
   qRegisterMetaType<SecureBinaryData>();

   timer_.setInterval(pollInterval ? pollInterval * 1000 : 3 * 1000);
   connect(&timer_, &QTimer::timeout, this, &FrejaSign::onTimer);

   connect(&freja_, &FrejaREST::repliedInitSignRequest, this, &FrejaSign::onRepliedInitSignRequest);
   connect(&freja_, &FrejaREST::repliedSignRequestStatus, this, &FrejaSign::onRepliedSignRequestStatus);
   connect(&freja_, &FrejaREST::requestFailed, this, &FrejaSign::onRequestFailed);
   connect(&freja_, &FrejaREST::emptyReply, [this] { waitForReply_ = false; });
}

FrejaSign::~FrejaSign()
{
   while (waitForReply_) {
      QCoreApplication::processEvents();
      QThread::msleep(1);
   }
}

bool FrejaSign::start(const QString &userId, const QString &title, const QString &data)
{
   if (userId.isEmpty()) {
      return false;
   }
   stopped_ = false;
   signRef_.clear();
   const auto reqId_ = freja_.sendSignRequest(userId, title, data);
   if (!reqId_) {
      return false;
   }
   emit statusUpdated(tr("request sent"));
   logger_->debug("Freja sign started for {}:{}", userId.toStdString(), data.toStdString());
   return true;
}

void FrejaSign::stop(bool cancel)
{
   timer_.stop();
   stopped_ = true;
   if (cancel && !signRef_.isEmpty()) {
      waitForReply_ = true;
      freja_.cancelSignRequest(signRef_);
      logger_->debug("Freja sign cancelled for {}", signRef_.toStdString());
   }
   signRef_.clear();
}

void FrejaSign::onTimer()
{
   if (stopped_ || signRef_.isEmpty() || reqId_) {
      return;
   }
   const auto reqId_ = freja_.requestSignStatus(signRef_);
   if (!reqId_) {
      logger_->error("[FrejaSign] failed to request status");
   }
}

void FrejaSign::onRepliedInitSignRequest(FrejaREST::SeqNo, const QString &signRef)
{
   signRef_ = signRef;
   reqId_ = 0;
   timer_.start();
}

void FrejaSign::onRepliedSignRequestStatus(FrejaREST::SeqNo, const QString &status, const QByteArray &signature)
{
   if (stopped_) {
      return;
   }
   reqId_ = 0;
   if (status == sApproved) {
      stop();
      onReceivedSignature(signature);
   }
   else if (status == sCancelled) {
      stop();
      emit failed(tr("cancelled by user"));
   }
   else if (status == sRejected) {
      stop();
      emit failed(tr("rejected"));
   }
   else if (status == sExpired) {
      stop();
      emit failed(tr("timed out"));
   }
   else if (status_ != status) {
      emit statusUpdated(status);
   }
   status_ = status;
}

void FrejaSign::onReceivedSignature(const QByteArray &signature)
{
   emit succeeded(SecureBinaryData(signature.toStdString()));
}

void FrejaSign::onRequestFailed(FrejaREST::SeqNo)
{
   emit failed(tr("request failed"));
   stop(true);
}


bool FrejaSignWallet::start(const QString &userId, const QString &title, const std::string &walletId)
{
   const auto data = QString::fromStdString("Wallet ID: " + walletId);
   return FrejaSign::start(userId, title, data);
}

void FrejaSignWallet::onReceivedSignature(const QByteArray &signature)
{
   const auto secureSig = SecureBinaryData(signature.toStdString());
   const auto password = secureSig.getSliceCopy(64, 32);
   assert(password.getSize() == 32);
   emit succeeded(password);
}


bool FrejaSignOTP::start(const QString &userId, const QString &title, const QString &otpId)
{
   const auto data = QLatin1String("OTP ID: ") + otpId;
   return FrejaSign::start(userId, title, data);
}

void FrejaSignOTP::onReceivedSignature(const QByteArray &signature)
{
   const auto secureSig = SecureBinaryData(signature.toStdString());
   const auto password = secureSig.getSliceCopy(96, 16);
   assert(password.getSize() == 16);
   emit succeeded(password);
}
