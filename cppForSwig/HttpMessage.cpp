////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2016, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include "HttpMessage.h"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#if defined(__APPLE__)
#include <cstdlib>
#endif

using namespace std;

///////////////////////////////////////////////////////////////////////////////
//
// HttpMessage
//
///////////////////////////////////////////////////////////////////////////////
void HttpMessage::setupHeaders()
{
   addHeader("POST / HTTP/1.1");
   stringstream addrHeader;
   addrHeader << "Host: " << addr_;
   addHeader(addrHeader.str());
   addHeader("Content-type: text/html; charset=UTF-8");
   addHeader("Connection: Keep-Alive");
}

///////////////////////////////////////////////////////////////////////////////
void HttpMessage::addHeader(string header)
{
   //headers should not have the termination CRLF
   header.append("\r\n");
   headers_.push_back(move(header));
}

///////////////////////////////////////////////////////////////////////////////
int32_t HttpMessage::makeHttpPayload(
   char** payload_out, const char* payload_in, size_t len)
{
   if (payload_out == nullptr)
      return -1;

   stringstream ss;
   ss << "Content-Length: ";
   ss << strlen(payload_in);
   ss << "\r\n\r\n";

   size_t httpHeaderSize = 0;
   for (auto& header : headers_)
      httpHeaderSize += header.size();

   *payload_out = new char[strlen(payload_in) +
      ss.str().size() +
      httpHeaderSize +
      1];

   int32_t pos = 0;
   for (auto& header : headers_)
   {
      memcpy(*payload_out + pos, header.c_str(), header.size());
      pos += header.size();
   }

   memcpy(*payload_out + pos, ss.str().c_str(), ss.str().size());
   pos += ss.str().size();

   memcpy(*payload_out + pos, payload_in, len);
   pos += len;

   memset(*payload_out + pos, 0, 1);
   return pos;
}
