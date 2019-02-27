#ifndef MDHS_CLIENT_H
#define MDHS_CLIENT_H

//#include <memory>
#include <QObject>
//#include "CommonTypes.h"

class ApplicationSettings;
class ConnectionManager;
namespace spdlog { class logger; }
class RequestReplyCommand;
namespace Blocksettle { namespace BS_MD { namespace TradeHistoryServer { class OhlcRequest; } } }

using namespace Blocksettle::BS_MD::TradeHistoryServer;

class MdhsClient : public QObject
{
   Q_OBJECT
public:
	//TODO: Move to proto file
	enum ProductType {
		ProductTypeUnknown = -1,
		ProductTypeFX,
		ProductTypeXBT,
		ProductTypePrivateMarket
	};
	MdhsClient(
	   const std::shared_ptr<ApplicationSettings>& appSettings,
	   const std::shared_ptr<ConnectionManager>& connectionManager,
	   const std::shared_ptr<spdlog::logger>& logger,
	   QObject* pParent = nullptr);

   ~MdhsClient() noexcept override = default;

   MdhsClient(const MdhsClient&) = delete;
   MdhsClient& operator = (const MdhsClient&) = delete;
   MdhsClient(MdhsClient&&) = delete;
   MdhsClient& operator = (MdhsClient&&) = delete;

   void SendRequest(const OhlcRequest& request);
   const ProductType GetProductType(const QString &product) const;

signals:
	void DataReceived(const std::string& data);

private:
	const bool OnDataReceived(const std::string& data);

	std::shared_ptr<ApplicationSettings>	appSettings_;
	std::shared_ptr<ConnectionManager>		connectionManager_;
	std::shared_ptr<spdlog::logger>			logger_;
	std::shared_ptr<RequestReplyCommand>	command_;
};
 
#endif // MDHS_CLIENT_H
