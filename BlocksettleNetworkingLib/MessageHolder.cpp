/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MessageHolder.h"
#include <cstring>


void delete_data(void *data, void *hint)
{
    delete[] static_cast<char*>(data);
}

MessageHolder::MessageHolder()
{
   zmq_msg_init(&message);
}

MessageHolder::MessageHolder(const std::string& data)
{
   char* buffer = new char[data.size()];
   std::memcpy(buffer, data.c_str(), data.size());

   zmq_msg_init_data(&message, static_cast<void*>(buffer), data.size(), delete_data, nullptr);
}

MessageHolder::~MessageHolder() noexcept
{
   zmq_msg_close(&message);
}

MessageHolder::MessageHolder(MessageHolder&& oth)
{
   zmq_msg_init(&message);
   zmq_msg_move(&message, &oth);
}

bool MessageHolder::IsLast()
{
   return zmq_msg_more(&message) == 0;
}

void* MessageHolder::GetData()
{
   return zmq_msg_data(&message);
}

size_t MessageHolder::GetSize()
{
   return zmq_msg_size(&message);
}

zmq_msg_t* MessageHolder::operator & ()
{
   return &message;
}

std::string MessageHolder::ToString()
{
   return std::string(static_cast<char*>(GetData()), GetSize());
}

int MessageHolder::ToInt()
{
   return *(static_cast<int*>(zmq_msg_data(&message)));
}
