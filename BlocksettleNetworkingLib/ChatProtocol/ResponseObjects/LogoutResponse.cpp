#include "LogoutResponse.h"
using namespace Chat;

LogoutResponse::LogoutResponse()
   : Response (ResponseType::ResponseLogout)
{

}

void LogoutResponse::handle(ResponseHandler & handler)
{
   handler.OnLogoutResponse(*this);
}
