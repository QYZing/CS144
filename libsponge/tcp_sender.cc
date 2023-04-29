#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    TCPSegment t;
    if(!_syn_flag)
    {
        t.header().syn = true;
        loading_data(t); 
        _syn_flag = true;
        return ;
    }
    else 
    {
        loading_data(t);  
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
//! \returns `false` if the ackno appears invalid (acknowledges something the TCPSender hasn't sent yet)
bool TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) 
{    
    uint64_t dackno = unwrap(ackno , _isn , _recv_seq);
    if( dackno > _next_seqno )
    {
        return false;
    }
    else if(dackno < _recv_seq) 
    {
        return true;
    }  
    _receiver_window_size = window_size;
    while(dackno > _recv_seq)
    {
        _recv_seq += _now_tmp_data.front().length_in_sequence_space();
        _bytes_in_flight -= _now_tmp_data.front().length_in_sequence_space();
        _now_tmp_data.pop();
        _retransmission_timeout = _initial_retransmission_timeout;
        _retransmission_count = 0;
    }
    if(_now_tmp_data.size()) 
    {
        _tick_flag = 1;
        _now_timer = 0;
    }
    fill_window();
    return true;
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick)
{
    if(!_tick_flag)
    {  
         _now_timer = 0;
        return ;
    }
    _now_timer += ms_since_last_tick;
    if(_now_timer >= _retransmission_timeout &&  _now_tmp_data.size())
    {
        _segments_out.push(_now_tmp_data.front());
        _retransmission_count ++;
        _retransmission_timeout *= 2;
        _now_timer = 0;
        _tick_flag = 1;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_count; }

void TCPSender::send_empty_segment()//only emtpy seqno
{
    TCPSegment t ;
    t.header().seqno = wrap(_next_seqno ,_isn);;
    _segments_out.push(t);
}

void TCPSender::loading_data(TCPSegment & t )
{
    if(_fin_flag ) return ;
    t.header().seqno = wrap(_next_seqno ,_isn); 
    string data ="";
    if(_syn_flag )
    {
        if(_stream.buffer_empty() && !_stream.eof() ) return;
        size_t size = _receiver_window_size == 0 ? 1 :_receiver_window_size;
        size = size - _bytes_in_flight;
        if(size <  1 )  return ;// dont have data size 
        if(_stream.buffer_size())
        {
            data = _stream.peek_output(size);
            _stream.pop_output(data.size()); 
            size  = size - data.size();
            t.payload() = Buffer(move(data));  
        }
        if(_stream.eof() &&  size)
        {
            t.header().fin = 1;
            _fin_flag = 1;
        }
    }
    _bytes_in_flight += t.length_in_sequence_space();    
    _next_seqno += t.length_in_sequence_space();
    _now_tmp_data.push(t);
    _segments_out.push(t);

    if(!_tick_flag)
    {
        _retransmission_timeout = _initial_retransmission_timeout;
        _tick_flag = 1;
        _now_timer = 0;
    }
}