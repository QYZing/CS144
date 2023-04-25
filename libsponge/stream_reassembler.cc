#include "stream_reassembler.hh"
#include<iostream>
#define FLAG 1
#define FF if(FLAG)
// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) :
                     _output(capacity), _capacity(capacity)
                     , _end_index(capacity) {
    _assembles.resize(_capacity);
    _set.resize(_capacity);
    for (auto &i : _assembles)
        i = 0;
    for(auto i : _set)
        i = 0;
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    size_t end = data.size() + index;
    size_t start = index >  _now_index ? index : _now_index;
    if(end > _capacity) end = _capacity;
    for(size_t i = start ; i < end ; i++)
    {
        if(_set[i] == 0)
        {
            _set[i] = 1;
            _assembles[i] = data[i - index];
            ++_sum_bytes;
        }
        if(_now_index == i)
        {
            _now_index = end;
        }
    }
    while(_now_index < _capacity &&  1 == _set[_now_index])
    {
        _now_index++;
    }
    if(_now_assem_byteix != _now_index)
    {
        string datatmp;
        for(size_t i = _now_assem_byteix ; i < _now_index ; i++)
            datatmp += _assembles[i];
        _now_assem_byteix = _now_index;
        _output.write(datatmp);
    }
    if(1 == eof) _end_index = end;
    if(_end_index == _now_index)
    {
        _output.end_input();
    }
}
size_t StreamReassembler::unassembled_bytes() const {
   return _sum_bytes - _now_index;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
