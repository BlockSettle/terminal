#ifndef OTC_CLIENT_H
#define OTC_CLIENT_H

#include <memory>
#include <unordered_map>
#include <QObject>

#include "BinaryData.h"
#include "OtcTypes.h"

namespace spdlog {
   class logger;
}

namespace Blocksettle { namespace Communication { namespace Otc {
   class Message;
   class Message_Offer;
   class Message_Accept;
   class Message_Close;
}}}


class OtcClient : public QObject
{
   Q_OBJECT
public:
   OtcClient(const std::shared_ptr<spdlog::logger> &logger, QObject *parent = nullptr);

   const bs::network::otc::Peer *peer(const std::string &peerId) const;

   bool sendOffer(const bs::network::otc::Offer &offer, const std::string &peerId);
   bool pullOrRejectOffer(const std::string &peerId);
   bool acceptOffer(const bs::network::otc::Offer &offer, const std::string &peerId);
   bool updateOffer(const bs::network::otc::Offer &offer, const std::string &peerId);

public slots:
   void peerConnected(const std::string &peerId);
   void peerDisconnected(const std::string &peerId);
   void processMessage(const std::string &peerId, const BinaryData &data);

signals:
   void sendMessage(const std::string &peerId, const BinaryData &data);

   void peerUpdated(const std::string &peerId);

private:
   void processOffer(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_Offer &msg);
   void processAccept(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_Accept &msg);
   void processClose(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message_Close &msg);

   void blockPeer(const std::string &reason, bs::network::otc::Peer *peer);

   bs::network::otc::Peer *findPeer(const std::string &peerId);

   void send(bs::network::otc::Peer *peer, const Blocksettle::Communication::Otc::Message &msg);

   std::shared_ptr<spdlog::logger> logger_;
   std::unordered_map<std::string, bs::network::otc::Peer> peers_;
};

#endif
