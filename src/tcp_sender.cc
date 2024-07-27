#include "tcp_sender.hh"
#include "tcp_config.hh"

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return retransmissionTimer_.getSequenceNumbersInFlight();
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return retransmissionTimer_.getConsecutiveRetransmissions();
}

void TCPSender::push( const TransmitFunction& transmit )
{
  // save transmit for close
  if (writer().is_closed() || !getCanSentFin()) {
    saveTransFunc_ = transmit;
  }
  while ((peer_win_size_ - sequence_numbers_in_flight() > 0 && reader().bytes_buffered() > 0) || !has_isn_ || getCanSentFin()) {
    uint64_t trans_len = min(min(TCPConfig::MAX_PAYLOAD_SIZE, peer_win_size_ - sequence_numbers_in_flight()), reader().bytes_buffered());
    string payload {};
    uint64_t index = reader().bytes_popped();
    // if not sent syn, sent
    // if sent syn, when buffer is not empty, sent
    {
      if (has_isn_)
        read(input_.reader(), trans_len, payload);
      TCPSenderMessage msg = {
        .seqno = Wrap32::wrap(index + has_isn_, isn_),
        .SYN = !has_isn_,
        .payload = payload,
        .FIN = getCanSentFin(),
        .RST = reader().has_error()
      };
      if (!has_isn_) {
        has_isn_ = true;
      }
      if (!has_fin_ && getCanSentFin()) {
        has_fin_ = true;
      }
      // save copy
      retransmissionTimer_.insertAcknoList(msg, cur_ms_);
      transmit(msg);
    }
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  return {
    .seqno = { Wrap32::wrap( has_isn_ + reader().bytes_popped() + has_fin_, isn_ ) },
    .SYN = false,
    .payload = {},
    .FIN = false,
    .RST = reader().has_error()
  };
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  if (msg.RST) {
    input_.writer().close();
    input_.reader().set_error();
    has_isn_ = false;
    peer_win_size_ = {};
  } else {
    if (msg.window_size) {
      peer_win_size_ = msg.window_size;
    } else {
      peer_win_size_ = 1;
      retransmissionTimer_.updateWinNonZero(0);
    }
    if (has_isn_ && msg.ackno.has_value()) {
      auto flag = retransmissionTimer_.updateAcknoList(msg.ackno->unwrap(isn_, reader().bytes_popped() + has_isn_), isn_, reader().bytes_popped() + has_isn_);
      if (flag) {
        retransmissionTimer_.resetRTOms(initial_RTO_ms_);
        retransmissionTimer_.resetTimer(cur_ms_);
        retransmissionTimer_.resetConsecutiveRetransmissions();
      }
      largest_ackno = max(largest_ackno, msg.ackno->unwrap(isn_, reader().bytes_popped() + has_isn_));
    }
  }
  if (getCanSentFin()) {
    push(saveTransFunc_);
  }

}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  cur_ms_ += ms_since_last_tick;
  retransmissionTimer_.updateRetransmissionTimer(cur_ms_, transmit);
}

void TCPSender::RetransmissionTimer::updateRetransmissionTimer( uint64_t cur_ms, const TransmitFunction& transmit )
{
  auto it = expected_ackno_list_.begin();
  for ( ; it != expected_ackno_list_.end(); ++it ) {
    if (cur_ms - it->start_ms_ >= cur_RTO_ms_) {
      transmit(it->mes_);
      it->start_ms_ = cur_ms;
      if (win_nonzero_) {
        consecutive_retransmissions_++;
        cur_RTO_ms_ *= 2;
      }

    }
    break;
  }
}

bool TCPSender::RetransmissionTimer::updateAcknoList(uint64_t ackno, Wrap32& isn, uint64_t checkpoint) {
  bool has_remove_outstanding = true;
  bool is_first = true;
  auto it = expected_ackno_list_.begin();
  // beyond next seqno
  if (it != expected_ackno_list_.end()) {
    auto& last_msg = expected_ackno_list_.back();
    if (ackno > last_msg.mes_.seqno.unwrap(isn, checkpoint) + last_msg.mes_.sequence_length()) {
      return false;
    }
  }
  for ( ; it != expected_ackno_list_.end(); ) {
    uint64_t abs_seqno = it->mes_.seqno.unwrap(isn, checkpoint);
    uint64_t seq_num = it->mes_.sequence_length();
    if (ackno <= abs_seqno) {
      if (is_first) {
        has_remove_outstanding = false;
      }
      break;
    } else {
      is_first = false;
      if (abs_seqno + seq_num <= ackno) {
        it = expected_ackno_list_.erase(it);
      } else if (ackno > abs_seqno) {
        break;
//        auto& msg = it->mes_;
//        if (msg.SYN) {
//          msg.SYN = false;
//          abs_seqno++;
//        }
//        auto payload_len = msg.payload.length();
//        auto payload_offset = min(ackno - abs_seqno, payload_len);
//        msg.payload = msg.payload.substr(payload_offset);
//        abs_seqno += payload_offset;
//        msg.seqno = Wrap32::wrap(abs_seqno, isn);
      }
    }
  }
  return has_remove_outstanding;
}
void TCPSender::RetransmissionTimer::insertAcknoList( TCPSenderMessage msg_, uint64_t start_ms_ )
{
  expected_ackno_list_.emplace_back(msg_, start_ms_);
}
