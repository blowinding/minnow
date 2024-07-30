#include "tcp_receiver.hh"
#include <iostream>

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  auto& seq = message.seqno;
  uint64_t offset = 0;
  if ( message.SYN ) {
    ISN_ = message.seqno;
    has_ISN_ = true;
    offset = 1;
  }
  if ( has_ISN_ ) {
    auto checkpoint = writer().bytes_pushed() + 1;
    reassembler_.insert( seq.unwrap( ISN_, checkpoint ) - 1 + offset, message.payload, message.FIN || message.RST );
  }
  ackno_ = has_ISN_ + writer().bytes_pushed() + writer().is_closed();
  if ( message.RST ) {
    has_ISN_ = false;
    reader().set_error();
  }
}

TCPReceiverMessage TCPReceiver::send() const
{
  bool RST = reader().has_error() || writer().has_error();
  std::optional<Wrap32> ackno;
  if ( has_ISN_ ) {
    ackno.emplace( Wrap32::wrap( ackno_, ISN_ ) );
  }
  return {
    ackno, static_cast<uint16_t>( min( writer().available_capacity(), static_cast<uint64_t> UINT16_MAX ) ), RST };
}
