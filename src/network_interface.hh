#pragma once

#include <queue>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <concepts>

#include "address.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"

// A "network interface" that connects IP (the internet layer, or network layer)
// with Ethernet (the network access layer, or link layer).

// This module is the lowest layer of a TCP/IP stack
// (connecting IP with the lower-layer network protocol,
// e.g. Ethernet). But the same module is also used repeatedly
// as part of a router: a router generally has many network
// interfaces, and the router's job is to route Internet datagrams
// between the different interfaces.

// The network interface translates datagrams (coming from the
// "customer," e.g. a TCP/IP stack or router) into Ethernet
// frames. To fill in the Ethernet destination address, it looks up
// the Ethernet address of the next IP hop of each datagram, making
// requests with the [Address Resolution Protocol](\ref rfc::rfc826).
// In the opposite direction, the network interface accepts Ethernet
// frames, checks if they are intended for it, and if so, processes
// the the payload depending on its type. If it's an IPv4 datagram,
// the network interface passes it up the stack. If it's an ARP
// request or reply, the network interface processes the frame
// and learns or replies as necessary.
template<typename T>
concept isDgram = requires(T t, Serializer& s) {
  { t.serialize(s) } -> std::same_as<void>;
};
class NetworkInterface
{
public:
  // An abstraction for the physical output port where the NetworkInterface sends Ethernet frames
  class OutputPort
  {
  public:
    virtual void transmit( const NetworkInterface& sender, const EthernetFrame& frame ) = 0;
    virtual ~OutputPort() = default;
  };

  // Construct a network interface with given Ethernet (network-access-layer) and IP (internet-layer)
  // addresses
  NetworkInterface( std::string_view name,
                    std::shared_ptr<OutputPort> port,
                    const EthernetAddress& ethernet_address,
                    const Address& ip_address );

  // Sends an Internet datagram, encapsulated in an Ethernet frame (if it knows the Ethernet destination
  // address). Will need to use [ARP](\ref rfc::rfc826) to look up the Ethernet destination address for the next
  // hop. Sending is accomplished by calling `transmit()` (a member variable) on the frame.
  void send_datagram( const InternetDatagram& dgram, const Address& next_hop );

  // Receives an Ethernet frame and responds appropriately.
  // If type is IPv4, pushes the datagram to the datagrams_in queue.
  // If type is ARP request, learn a mapping from the "sender" fields, and send an ARP reply.
  // If type is ARP reply, learn a mapping from the "sender" fields.
  void recv_frame( const EthernetFrame& frame );

  // Called periodically when time elapses
  void tick( size_t ms_since_last_tick );

  // Accessors
  const std::string& name() const { return name_; }
  const OutputPort& output() const { return *port_; }
  OutputPort& output() { return *port_; }
  std::queue<InternetDatagram>& datagrams_received() { return datagrams_received_; }

  // ethernet address and timeout
  struct MacAddrUnit {
    EthernetAddress mac_addr_ {};
    uint64_t learning_time_ {};
  };

private:
  // Human-readable name of the interface
  std::string name_;

  // The physical output port (+ a helper function `transmit` that uses it to send an Ethernet frame)
  std::shared_ptr<OutputPort> port_;
  void transmit( const EthernetFrame& frame ) const { port_->transmit( *this, frame ); }

  // Ethernet (known as hardware, network-access-layer, or link-layer) address of the interface
  EthernetAddress ethernet_address_;

  // IP (known as internet-layer or network-layer) address of the interface
  Address ip_address_;

  // Datagrams that have been received
  std::queue<InternetDatagram> datagrams_received_ {};

  constexpr static uint64_t ARP_INTERVAL_ =  5000;
  constexpr static uint64_t ARP_TIMEOUT_  = 30000;

  // current time
  uint64_t cur_time_ {};

  // map between mac address and ip address
  std::unordered_map<uint32_t, MacAddrUnit> map_ip_ {};

  // map between ip address and ipv4 dgram queue
  std::unordered_map<uint32_t, std::queue<InternetDatagram>> map_queue_ {};

  // map between ip address and arp send time
  std::unordered_map<uint32_t, uint64_t> map_send_time_ {};

  template<isDgram T>
  static void datagramToEthernetFrame(EthernetFrame& ethernetFrame, const T& internetDatagram, const EthernetAddress& dst, const EthernetAddress& src, const uint64_t& type) {
    Serializer serializer;
    internetDatagram.serialize(serializer);
    // header
    auto& header = ethernetFrame.header;
    header.src = src;
    header.dst = dst;
    header.type = type;
    // frame
    ethernetFrame.payload = serializer.output();
  }

  static ARPMessage genArpEthernetFrame(uint16_t opcode, const EthernetAddress& src, const EthernetAddress& dst,
                                         const uint32_t src_ip, const uint32_t dst_ip,
                                         uint16_t hardwareType = ARPMessage::TYPE_ETHERNET, uint16_t protocolType = EthernetHeader::TYPE_IPv4) {
    ARPMessage arpMessage {};
    arpMessage.hardware_type = hardwareType;
    arpMessage.protocol_type = protocolType;
    arpMessage.opcode = opcode;
    arpMessage.sender_ip_address = src_ip;
    arpMessage.target_ip_address = dst_ip;
    arpMessage.sender_ethernet_address = src;
    arpMessage.target_ethernet_address = dst;
    return arpMessage;
  }

  void transmitDgramInQueue(uint32_t sender_ip_address, const EthernetAddress& senderMac) {
    auto& senderQueue = map_queue_[sender_ip_address];
    while (!senderQueue.empty()) {
      auto& dgram = senderQueue.front();
      EthernetFrame frame {};
      datagramToEthernetFrame(frame, dgram, senderMac, ethernet_address_, EthernetHeader::TYPE_IPv4);
      transmit(frame);
      senderQueue.pop();
    }
  }


};
