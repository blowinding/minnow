#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  router_table_.emplace_back( route_prefix, prefix_length, next_hop, interface_num );
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  for ( auto& item : _interfaces ) {
    auto& dgram_queue = item->datagrams_received();
    while ( !dgram_queue.empty() ) {
      auto& dgram = dgram_queue.front();
      // check TTL
      int ttl = static_cast<int>( dgram.header.ttl );
      dgram.header.ttl = static_cast<uint8_t>( max( ttl - 1, 0 ) );
      dgram.header.compute_checksum();
      // if TTL>0, route
      if ( dgram.header.ttl != 0 ) {
        routeHelperFunc( dgram );
      }
      dgram_queue.pop();
    }
  }
}

void Router::routeHelperFunc( const InternetDatagram& dgram )
{
  // find proper interface
  uint32_t ip_numeric = dgram.header.dst;
  size_t route_interface = _interfaces.size();
  uint8_t max_prefix = 0;
  std::optional<Address> next_hop;
  for ( auto& item : router_table_ ) {
    uint8_t cur_prefix_len = item.prefix_length_;
    uint32_t ip_comp = cur_prefix_len > 0 ? ip_numeric >> ( 32 - cur_prefix_len ) : 0;
    uint32_t prefix_comp = cur_prefix_len > 0 ? item.route_prefix_ >> ( 32 - cur_prefix_len ) : 0;
    if ( ip_comp == prefix_comp ) {
      if ( cur_prefix_len >= max_prefix ) {
        route_interface = item.interface_num_;
        max_prefix = cur_prefix_len;
        next_hop = item.next_hop_;
      }
    }
  }
  if ( route_interface < _interfaces.size() ) {
    auto& interface = _interfaces[route_interface];
    if ( next_hop.has_value() ) {
      interface->send_datagram( dgram, next_hop.value() );
    } else {
      interface->send_datagram( dgram, Address::from_ipv4_numeric( ip_numeric ) );
    }
  }
}
