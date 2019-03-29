#include "ContactsListResponse.h"
using namespace Chat;

ContactsListResponse::ContactsListResponse(std::vector<std::string> dataList)
   : ListResponse (ResponseType::ResponseContactsList, dataList)
{
   contactsList_.reserve(dataList_.size());
   for (const auto& contactData : dataList_){
      contactsList_.push_back(ContactRecordData::fromJSON(contactData));
   }
}

ContactsListResponse::ContactsListResponse(std::vector<std::shared_ptr<ContactRecordData> > contactList)
   :ListResponse (ResponseType::ResponseContactsList, {})
   ,contactsList_(contactList)
{
   std::vector<std::string> contacts;
   for (const auto& contact: contactsList_){
      contacts.push_back(contact->toJsonString());
   }
   dataList_ = std::move(contacts);
}

std::shared_ptr<Response> ContactsListResponse::fromJSON(const std::string &jsonData)
{
   QJsonObject data = QJsonDocument::fromJson(QString::fromStdString(jsonData).toUtf8()).object();
   return std::make_shared<ContactsListResponse>(ListResponse::fromJSON(jsonData));
}

void ContactsListResponse::handle(ResponseHandler & handler)
{
   handler.OnContactsListResponse(*this);
}

const std::vector<std::shared_ptr<ContactRecordData> > &ContactsListResponse::getContactsList() const
{
   return contactsList_;
}
