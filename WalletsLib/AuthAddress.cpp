#include "AuthAddress.h"

AuthAddress::AuthAddress(const bs::Address &chainedAddr, AddressVerificationState state)
   : chainedAddress_(chainedAddr)
   , state_(state)
{}

std::string to_string(AddressVerificationState state)
{
   static const std::string stateStrings[] = { "VerificationFailed"
                                             , "InProgress"
                                             , "NotSubmitted"
                                             , "Submitted"
                                             , "PendingVerification"
                                             , "VerificationSubmitted"
                                             , "Verified"
                                             , "Revoked"
                                             , "RevokedByBS"};

   return stateStrings[(size_t)state];
}
