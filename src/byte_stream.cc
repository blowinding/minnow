#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {
  is_close_ = false;
  error_ = false;
}

bool Writer::is_closed() const
{
  return is_close_;
}

void Writer::push( string data )
{
  if (is_closed()) {
    return;
  }
  auto len = data.size();
  auto push_len = min(available_capacity(), len);
  if (push_len > 0) {
    real_queue.emplace_back(data.substr(0, push_len));
    string& top_str = real_queue.back();
    view_queue.emplace_back(top_str);
    buffered_+=push_len;
    bytes_pushed_+=push_len;
  }
}

void Writer::close()
{
  is_close_ = true;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ > buffered_ ? capacity_ - buffered_ : 0;
}

uint64_t Writer::bytes_pushed() const
{
  return bytes_pushed_;
}

bool Reader::is_finished() const
{
  return is_close_ && buffered_ == 0;
}

uint64_t Reader::bytes_popped() const
{
  return bytes_popped_;
}

string_view Reader::peek() const
{
  if (buffered_ > 0) {
    return view_queue.front();
  } else {
    return {};
  }
}

void Reader::pop( uint64_t len )
{
    while (len > 0 && buffered_ > 0) {
      string_view& view_str = view_queue.front();
      auto len_str = view_str.size();
      auto len_pop = min(len, len_str);
      if (len >= len_str) {
        view_queue.pop_front();
        real_queue.pop_front();
      } else {
        view_str.remove_prefix(len);
      }
      buffered_-=len_pop;
      bytes_popped_+=len_pop;
      len-=len_pop;
    }

}

uint64_t Reader::bytes_buffered() const
{
  return buffered_;
}
