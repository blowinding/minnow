#include "reassembler.hh"
#include <iostream>
using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  auto pushed_num = output_.writer().bytes_pushed();
  auto capacity_num = output_.writer().available_capacity();
  auto data_len = data.size();

  // discard bytes out of range
  auto pushed_first_index = max(pushed_num, first_index);
  auto pushed_data_len = min(data_len, capacity_num - pushed_first_index + pushed_num);
  if (pushed_first_index - first_index > data_len || pushed_first_index >= pushed_num + capacity_num) {
    return;
  } else {
    data = data.substr(pushed_first_index - first_index, pushed_data_len);
    if (pushed_first_index - first_index + pushed_data_len < data_len) {
      is_last_substring = false;
    }
  }

  // insert bytes
  reassembler_.insert_bytes_list(pushed_first_index, data, is_last_substring);

  // push data into bytestream
  auto unpushed_first_index = reassembler_.unpushed_first_index();

  if (unpushed_first_index == pushed_num) {
    // when the first index can properly put into bytestream
    std::string pushed_data {};
    auto res = reassembler_.delete_bytes_list(capacity_num, pushed_data);
    output_.writer().push(pushed_data);
    if (res) {
      output_.writer().close();
    }
  }

  pending_bytes_ = reassembler_.getPendingLen();

}

uint64_t Reassembler::bytes_pending() const
{
  return pending_bytes_;
}

void Reassembler::PendingBytes::insert_bytes_list( uint64_t first_index, string data, bool is_last_substring )
{
  auto it_list = bytes_list_.begin();
  auto data_len = data.size();
  bool can_merge = false;
  // insert data initially
  for ( ; it_list != bytes_list_.end(); ++it_list ) {
    uint64_t& cur_first_index = it_list->first_index_;
    string& cur_data = it_list->data_;
    auto cur_data_len = cur_data.size();
    if (first_index + data_len < cur_first_index) {
      it_list = bytes_list_.insert(it_list, {first_index, data, is_last_substring});
      break;
    } else if (first_index > cur_first_index + cur_data_len) {
      continue;
    } else {
      string final_data = {};
      uint64_t final_first_index = 0;
      getMergedData(cur_first_index, first_index, cur_data, data, final_first_index, final_data);
      cur_first_index = final_first_index;
      cur_data = final_data;
      can_merge = true;
      break;
    }
  }

  // merge
  if (it_list == bytes_list_.end()) {
    bytes_list_.insert(it_list, {first_index, data, is_last_substring});
  } else {
    if (can_merge) {
      auto& top_data = it_list->data_;
      auto top_index = it_list->first_index_;
      auto top_data_len = top_data.length();
      it_list++;
      for ( ; it_list != bytes_list_.end(); ) {
        uint64_t& cur_first_index = it_list->first_index_;
        string& cur_data = it_list->data_;
        if (top_index + top_data_len < cur_first_index) {
          break;
        } else {
          string final_data = {};
          uint64_t final_first_index = 0;
          getMergedData( cur_first_index, top_index, cur_data, top_data, final_first_index, final_data );
          top_index = final_first_index;
          top_data = final_data;
          it_list = bytes_list_.erase(it_list);
        }
      }
    }
  }


  update_unpushed_first_index(bytes_list_.front().first_index_);
}

bool Reassembler::PendingBytes::delete_bytes_list( uint64_t max_len, string& data )
{
  bool is_last = false;

  auto& view_str = bytes_list_.front().data_;
  auto view_str_len = view_str.length();
  auto pushed_len = min(max_len, view_str_len);

  data.assign(view_str, 0, pushed_len);

  // delete pending str
  auto it_list = bytes_list_.begin();
  is_last = it_list->is_last_substring_;
  bytes_list_.erase(it_list);

  return is_last ;
}

