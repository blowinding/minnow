#pragma once

#include "byte_stream.hh"
#include <list>
#include <utility>
#include <iostream>


class Reassembler
{
public:
  // Construct Reassembler to write into given ByteStream.
  explicit Reassembler( ByteStream&& output ) : output_( std::move( output ) ) {}

  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;

  // Access output stream reader
  Reader& reader() { return output_.reader(); }
  const Reader& reader() const { return output_.reader(); }

  // Access output stream writer, but const-only (can't write from outside)
  const Writer& writer() const { return output_.writer(); }

  class PendingBytes{
    uint64_t unpushed_first_index_ {};
    struct PendingBytesUnit{
      uint64_t first_index_;
      bool is_delete_;
      bool is_last_substring_;
      std::string data_;
      PendingBytesUnit(uint64_t first_index, std::string data, bool is_last_substring = false)
        : first_index_(first_index), is_delete_(false), is_last_substring_(is_last_substring), data_(std::move(data)) {}
    };
    std::list<PendingBytesUnit> bytes_list_ {};
  public:
    PendingBytes() {};
    void insert_bytes_list( uint64_t first_index, std::string data, bool is_last_substring );
    bool delete_bytes_list( uint64_t max_len, std::string& data);
    uint64_t unpushed_first_index() const {
      return unpushed_first_index_;
    }
    void update_unpushed_first_index(uint64_t index) {
      unpushed_first_index_ = index;
    }
    uint64_t getPendingLen() {
      uint64_t len = 0;
      for (const auto& i: bytes_list_) {
        len += i.data_.length();
      }
      return len;
    }
    void getMergedData(uint64_t cur_first_index, uint64_t first_index, string& cur_data, string& data, uint64_t& final_first_index, string& final_data) {
      auto cur_data_len = cur_data.length();
      auto data_len = data.length();
      final_first_index = min(cur_first_index, first_index);
      uint64_t final_last_index = max(cur_first_index + cur_data_len, first_index + data_len) - 1;
      uint64_t final_data_len = final_last_index - final_first_index + 1;
      uint64_t left_intersect = max(cur_first_index, first_index);
      uint64_t right_intersect = min(cur_first_index + cur_data_len, first_index + data_len) - 1;
      for ( uint64_t i = 0; i < final_data_len; ++i ) {
        if (cur_first_index + i < left_intersect) {
          final_data.push_back(cur_data[i]);
        } else if (first_index + i < left_intersect) {
          final_data.push_back(data[i]);
        } else if (final_first_index + i >= left_intersect && final_first_index + i <= right_intersect && left_intersect <= right_intersect) {
          final_data.push_back(data[final_first_index + i - first_index]);
        } else if (final_first_index + i > cur_first_index + cur_data_len - 1) {
          final_data.push_back(data[final_first_index + i - first_index]);
        } else if (final_first_index + i > first_index + data_len - 1) {
          final_data.push_back(cur_data[final_first_index + i - cur_first_index]);
        }
      }
    }
    // debug
    void printPendingBytes() {
      int cnt = 0;
      for (const auto& i: bytes_list_) {
        cout << "     bytes_list_ pending bytes" << endl;
        cout << "***********" << cnt << "***********" << endl;
        cout << "      data:";
        for (const auto &item : i.data_) {
          cout << "0x";
          cout << hex << int(item);
          cout << ",";
        }
        cout << endl;
        cout << "first index" << i.first_index_ << endl;
        cnt++;
      }
    }
  };
private:
  ByteStream output_; // the Reassembler writes to this ByteStream
  uint64_t pending_bytes_ {};
  PendingBytes reassembler_ {};
};
