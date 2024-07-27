#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"

#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <optional>
#include <queue>

class TCPSender
{
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( ByteStream&& input, Wrap32 isn, uint64_t initial_RTO_ms )
    : input_( std::move( input ) ), isn_( isn ), initial_RTO_ms_( initial_RTO_ms )
  {}

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage make_empty_message() const;

  /* Receive and process a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Type of the `transmit` function that the push and tick methods can use to send messages */
  using TransmitFunction = std::function<void( const TCPSenderMessage& )>;

  /* Push bytes from the outbound stream */
  void push( const TransmitFunction& transmit );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called */
  void tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit );

  // Accessors
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
  Writer& writer() { return input_.writer(); }
  const Writer& writer() const { return input_.writer(); }

  // Access input stream reader, but const-only (can't read from outside)
  const Reader& reader() const { return input_.reader(); }

  /* Retransmission Timer */
  class RetransmissionTimer {
  public:
    RetransmissionTimer(uint64_t initial_RTO_ms): cur_RTO_ms_(initial_RTO_ms), expected_ackno_list_({}) {}
    struct AckWrapper{
      TCPSenderMessage mes_;
      uint64_t start_ms_;
    };
    void updateRetransmissionTimer(uint64_t cur_ms, const TransmitFunction& transmit);
    bool updateAcknoList(uint64_t ackno, Wrap32& isn, uint64_t checkpoint);
    void insertAcknoList(TCPSenderMessage msg_, uint64_t start_ms_);
    void updateWinNonZero(uint64_t peer_win_size) {
      win_nonzero_ = (peer_win_size != 0);
    }
    uint64_t getConsecutiveRetransmissions() const {
      return consecutive_retransmissions_;
    }
    uint64_t getSequenceNumbersInFlight() const {
      uint64_t sum = {};
      for (const auto& i: expected_ackno_list_) {
        sum += i.mes_.sequence_length();
      }
      return sum;
    }
    void resetConsecutiveRetransmissions() {
      consecutive_retransmissions_ = 0;
    }
    void resetRTOms(uint64_t initial_RTO_ms) {
      cur_RTO_ms_ = initial_RTO_ms;
    }
    void resetTimer(uint64_t cur_ms) {
      for (auto& i: expected_ackno_list_) {
        i.start_ms_ = cur_ms;
      }
    }
  private:
    uint64_t cur_RTO_ms_ {};
    uint64_t consecutive_retransmissions_ {};
    std::list<AckWrapper> expected_ackno_list_;
    bool win_nonzero_ { true };
  };
private:
  // Variables initialized in constructor
  ByteStream input_;
  Wrap32 isn_;
  uint64_t initial_RTO_ms_;

  // Variables used
  uint64_t cur_ms_ { };
  uint64_t peer_win_size_ { 1 };
  uint64_t largest_ackno {0};
  bool has_isn_ { false };
  bool has_fin_ { false };

  // Retransmission Timer
  RetransmissionTimer retransmissionTimer_ { initial_RTO_ms_ };

  // save transmit function for close
  TransmitFunction saveTransFunc_ {};
  bool getCanSentFin() const {
    return !has_fin_ && reader().is_finished() && (peer_win_size_ > reader().bytes_popped() + has_isn_ + reader().bytes_buffered() - largest_ackno);
  }
};
