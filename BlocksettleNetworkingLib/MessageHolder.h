/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __ZMQ_MESSAGE_HOLDER_H__
#define __ZMQ_MESSAGE_HOLDER_H__

#include <string>
#include <zmq.h>

class MessageHolder {
public:
    MessageHolder();
    MessageHolder(const std::string& data);
    ~MessageHolder() noexcept;
    MessageHolder(MessageHolder&&);

    MessageHolder(const MessageHolder&) = delete;
    MessageHolder& operator = (MessageHolder) = delete;

    MessageHolder& operator = (MessageHolder&&) = delete;

    bool IsLast();

    void*       GetData();
    size_t      GetSize();

    zmq_msg_t* operator & ();

    std::string ToString();
    int         ToInt();

private:
    zmq_msg_t   message;
};

#endif // __ZMQ_MESSAGE_HOLDER_H__
