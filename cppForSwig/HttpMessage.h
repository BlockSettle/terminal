////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_FCGI_MESSAGE_
#define _H_FCGI_MESSAGE_

#include <stdint.h>
#include <vector>
#include <string>
#include <sstream>
#include "BinaryData.h"

///////////////////////////////////////////////////////////////////////////////
class HttpMessage
{
private:
   std::vector<std::string> headers_;
   const std::string addr_;

public:
   HttpMessage(const std::string& addr, bool initHeaders = true) :
      addr_(addr)
   {
      if(initHeaders)
         setupHeaders();
   }

   void setupHeaders(void);
   void addHeader(std::string);
   int32_t makeHttpPayload(
      char** payload_out, const char* payload_in, size_t len);
};

#endif
