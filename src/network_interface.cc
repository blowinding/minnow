#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // get next_hop ip address
  uint32_t ip_numeric = next_hop.ipv4_numeric();
  auto it_res = map_ip_.find(ip_numeric);
  EthernetFrame frame {};
  // if not learning arp mapping or arp mapping timeout
  if (it_res == map_ip_.end() || cur_time_ - it_res->second.learning_time_ >= ARP_TIMEOUT_) {
    // cannot find phy address, queue frame, and send arp request
    map_queue_[ip_numeric].push(dgram);
    // check if arp request too frequently
    auto it_send_res = map_send_time_.find(ip_numeric);
    if (it_send_res != map_send_time_.end()) {
      if (cur_time_ - it_send_res->second <= ARP_INTERVAL_)
        return;
    } else {
      map_send_time_[ip_numeric] = cur_time_;
    }
    auto ARP_msg = genArpEthernetFrame(ARPMessage::OPCODE_REQUEST, ethernet_address_,
                                        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }, ip_address_.ipv4_numeric(), ip_numeric);
    datagramToEthernetFrame<ARPMessage>(frame, ARP_msg, ETHERNET_BROADCAST, ethernet_address_, EthernetHeader::TYPE_ARP);
  } else {
    // ipv4 dgram
    datagramToEthernetFrame<InternetDatagram>(frame, dgram, it_res->second.mac_addr_, ethernet_address_, EthernetHeader::TYPE_IPv4);
  }
  // transmit
  transmit(frame);
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  auto& header = frame.header;
  // check receive
  auto dst = header.dst;
  if (dst != ethernet_address_ && dst != ETHERNET_BROADCAST) {
    return;
  }
  auto& payload = frame.payload;
  Parser parser(payload);
  if (header.type == EthernetHeader::TYPE_IPv4) {
    // ipv4 dgram push into queue
    InternetDatagram dgram {};
    dgram.parse(parser);
    if (!parser.has_error()) {
      datagrams_received_.push(std::move(dgram));
    }
  } else if (header.type == EthernetHeader::TYPE_ARP) {
    // arp msg process
    ARPMessage arpMessage {};
    arpMessage.parse(parser);
    if (!parser.has_error()) {
      // if somebody request, send reply
      if(arpMessage.opcode == ARPMessage::OPCODE_REQUEST && arpMessage.target_ip_address == ip_address_.ipv4_numeric()) {
        ARPMessage arpReplyMsg = genArpEthernetFrame(ARPMessage::OPCODE_REPLY, ethernet_address_,
                                                      arpMessage.sender_ethernet_address, ip_address_.ipv4_numeric(), arpMessage.sender_ip_address);
        EthernetFrame replyFrame {};
        datagramToEthernetFrame<ARPMessage>(replyFrame, arpReplyMsg, arpMessage.sender_ethernet_address, ethernet_address_, EthernetHeader::TYPE_ARP);
        transmit(replyFrame);
      }
      map_ip_[arpMessage.sender_ip_address] = {
        .mac_addr_ = arpMessage.sender_ethernet_address,
        .learning_time_ = cur_time_
      };
      transmitDgramInQueue(arpMessage.sender_ip_address, arpMessage.sender_ethernet_address);
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  cur_time_ += ms_since_last_tick;
}
