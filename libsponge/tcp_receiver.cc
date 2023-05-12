#include "tcp_receiver.hh"
#include<iostream>
// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.
template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

bool TCPReceiver::segment_received(const TCPSegment &seg) {
    // syn and fin can possible  come here thar at the same time
    uint64_t index{};           // push_string index
    if (1 == seg.header().syn)  // deal syn
    {
        if (1 == _start_flag)
            return false;
        _start_flag = 1;
        _isn = seg.header().seqno.raw_value();
        _null_offset++;
        if (seg.length_in_sequence_space() == 1)
            return true;  // only syn
    } else {
        if (0 == _start_flag)
            return false;
        // @don`t have syn   then  deal flow push_string index
        index = unwrap(seg.header().seqno, WrappingInt32(_isn), _reassembler.headno()) - 1;
    }
    if (1 == seg.header().fin)  // deal fin
    {
        if (1 == _eof)
            return false;
        _eof = 1;
    }
    {
        uint64_t top = _reassembler.headno() + _capacity;
        uint64_t low = _reassembler.headno();
        uint64_t endindex = index + seg.payload().size();
        if (seg.payload().size())
            endindex--;
      //  cerr<<" index "<<index<<" top "<<top <<" low "<<low<<endl;
        if (index >= top || endindex < low )
            return false;  // not in window
       // cerr<<"上面的符合条件 "<<" index "<<index<<" top "<<top <<" low "<<low<<endl;
    }
 
    //@ imitate sliding window at twice puhs_string
    size_t redata_size = seg.payload().size();            // virtual size
    size_t now_remasmbler_size = _reassembler.residue();  // reality size
    if (now_remasmbler_size < redata_size) {
        string data_tmp = seg.payload().copy();
        _reassembler.push_substring(data_tmp.substr(0, now_remasmbler_size), index , 0);
        _reassembler.push_substring(data_tmp.substr(now_remasmbler_size , redata_size), index + now_remasmbler_size , seg.header().fin);
    } else {
        _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
    }
    if(_reassembler.stream_out().input_ended())
        _null_offset++;
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_start_flag)
        return {wrap(_reassembler.headno() + _null_offset, WrappingInt32(_isn))};
    return std::nullopt;
}

size_t TCPReceiver::window_size() const  // virtual size
{
    return _capacity - _reassembler.stream_out().buffer_size();
}
