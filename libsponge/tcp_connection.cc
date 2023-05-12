#include "tcp_connection.hh"

#include<iostream>
// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    if(_state==TCPState::State::CLOSE_WAIT)
    {
      //  cerr<<"close _wait 接收到消息 "<<seg.header().ackno<<endl;
    }
    //   _sender.ack_received(seg.header().ackno, seg.header().win);
    if (!_receiver.stream_out().eof() && !_receiver.segment_received(seg))
    {
        if(seg.header().ack && _state != TCPState::State::ESTABLISHED ){
            if ( !seg.header().rst &&_state == TCPState::State::SYN_RCVD ) {
                send(false);
            }
            else  if (seg.header().rst){
                unclean_shutdown(false);
            }
            // else if ( seg.payload().size() == 0 && (_state == TCPState::State::LISTEN 
            //         || _state == TCPState::State::SYN_SENT))
            // {
            //     _sender.send_empty_segment(seg.header().ackno);
            //     unclean_shutdown(true);
            //     return ;
            // }
            else if(_state == TCPState::State::FIN_WAIT_2)
            {
                send(false);
            }
            return;
        }    
        else if(seg.header().rst) return ;
        send(false);
        return;
    }
     _sender.ack_received(seg.header().ackno, seg.header().win);
    if(seg.header().rst){
        unclean_shutdown(false);
        return ;
    }
    else if (_state == TCPState::State::FIN_WAIT_1) {
        if (seg.header().fin && _sender.bytes_in_flight()== 0 ) {
                _state = TCPState::State::TIME_WAIT;
        } else if( !seg.header().fin ){
            _state = TCPState::State::FIN_WAIT_2;
            if (_receiver.stream_out().input_ended()) 
                _state = _state = TCPState::State::TIME_WAIT;
            return;
        }else 
        {
            //_state = TCPState::State::LAST_ACK;
        }
    } else if (_state == TCPState::State::FIN_WAIT_2 && seg.header().fin) {
            _state = TCPState::State::TIME_WAIT;

    }else if (_state == TCPState::State::LAST_ACK) {
        if (!_sender.bytes_in_flight())
            _state = TCPState::State::CLOSED;
        return;
    } else if (_state == TCPState::State::CLOSE_WAIT && !seg.header().fin) {
     //   _state = TCPState::State::CLOSED;
        return;
    }
    else if(_state == TCPState::State::TIME_WAIT)
    {
        if(seg.header().fin)
        {
           
        }
        else return ;
    }
    else if (_state == TCPState::State::ESTABLISHED) {
        if (seg.header().fin) {
            _state = TCPState::State::CLOSE_WAIT;
        } else if (seg.payload().size() == 0) {
            if (seg.header().ackno.raw_value() <= _sender.next_seqno().raw_value() && !_sender.segments_out().size()) 
                return;
        } 
    } else if (_state == TCPState::State::LISTEN) {
        if (seg.header().syn) {
            _state = TCPState::State::SYN_RCVD;
            send(true);
        }
        return;
    } else if (_state == TCPState::State::SYN_SENT && seg.header().syn) {
            _state = TCPState::State::ESTABLISHED;   
    } else if (_state == TCPState::State::SYN_RCVD) {
        _state = TCPState::State::ESTABLISHED;
        return;
    } 
    send(false);
}

bool TCPConnection::active() const { return _state != TCPState::State::CLOSED; }

size_t TCPConnection::write(const string &data) {
    size_t data_size = _sender.stream_in().write(data);
    send(true);
    return data_size;  
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active()) {
        return;
    }
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) { 
        unclean_shutdown(true);
    }
    send(true);
    if (_state == TCPState::State::TIME_WAIT && _time_since_last_segment_received >= _cfg.rt_timeout * 10) {
        _state = TCPState::State::CLOSED;
    }
}

void TCPConnection::end_input_stream() 
{
    if(_sender.stream_in().input_ended()) return ;
    _sender.stream_in().end_input();
    if (_state == TCPState::State::CLOSE_WAIT) {
        _state = TCPState::State::LAST_ACK;
    }else {
        _state = TCPState::State::FIN_WAIT_1;
    }
    send(true);
}

void TCPConnection::connect() {
    _state = TCPState::State::SYN_SENT;
    send(true);
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {         
            // Your code here: need to send a RST segment to the peer
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            unclean_shutdown(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::send(bool loading_window ) {
    if(_state == TCPState::State::LISTEN) return ;
    if (loading_window){
        _sender.fill_window();
    }else if(!_sender.segments_out().size()){
        _sender.send_empty_segment();
    }
    while (_sender.segments_out().size()) {
        TCPSegment t = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            t.header().win = _receiver.window_size();
            t.header().ack = 1;
            t.header().ackno = _receiver.ackno().value();
        }
        if(_state == TCPState::State::RESET)
        {
            t.header().rst = true;
            if(_sender.bytes_in_flight())
            {
                t.header().seqno = t.header().seqno - _sender.bytes_in_flight();
            }
        }
        _segments_out.push(move(t));

    }
    if (_receiver.stream_out().input_ended() && !(_sender.stream_in().eof())) {
        _linger_after_streams_finish = false;
    }
}
void TCPConnection::unclean_shutdown(bool is_send) {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();  
    if (is_send) 
    {  
        _state = TCPState::State::RESET;
        send(false);
    }
    _state = TCPState::State::CLOSED;
}

///  t_ucS_128K_8K_L 
