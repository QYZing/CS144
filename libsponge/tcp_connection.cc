#include "tcp_connection.hh"

#include <iostream>
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
    if (!_receiver.stream_out().eof() && !_receiver.segment_received(seg)) {
        if (seg.header().rst && _state == TCPState::State::ESTABLISHED)
            return;
        else if (seg.header().rst) {
            if (seg.header().ack) {
                unclean_shutdown(false);
            }
            return;
        }
        if (_state == TCPState::State::LISTEN && seg.header().ack) {
            return;
        } else if (_state == TCPState::State::SYN_RCVD && seg.header().ack) {
            _sender.send_empty_segment();
            send(false);
            return;
        } else if (_state != TCPState::State::ESTABLISHED && seg.header().ack)
            return;
        _sender.send_empty_segment();
        send(false);
        return;
    }
    _sender.ack_received(seg.header().ackno, seg.header().win);
    // FF cout<<"当前状态"<<_state.name()<<endl;
    if (seg.header().rst) {
        unclean_shutdown(false);
        return;
    } else if (_state == TCPState::State::FIN_WAIT_1) {
        if (seg.header().fin) {
            if (_sender.bytes_in_flight() == 0) {
                _state = TCPState::State::TIME_WAIT;
                _linger_after_streams_finish = true;
            }
            _receiver.stream_out().end_input();
            _sender.send_empty_segment();
        } else {
            _state = TCPState::State::FIN_WAIT_2;
            if (_receiver.stream_out().eof()) {
                _state = _state = TCPState::State::TIME_WAIT;
                if (_sender.bytes_in_flight()) {
                    // _sender.clear();
                }
            }
            return;
        }
    } else if (_state == TCPState::State::FIN_WAIT_2) {
        if (1 == seg.header().fin) {
            _state = TCPState::State::TIME_WAIT;
            _linger_after_streams_finish = true;
            _sender.send_empty_segment();
        }
    } else if (_state == TCPState::State::TIME_WAIT) {
        _sender.send_empty_segment();
    } else if (_state == TCPState::State::LAST_ACK) {
        if (!_sender.bytes_in_flight())
            _state = TCPState::State::CLOSED;
        return;
    } else if (_state == TCPState::State::CLOSE_WAIT) {
        if (seg.header().fin)
            _sender.send_empty_segment();
        else
            return;
    } else if (_state == TCPState::State::ESTABLISHED) {
        if (seg.header().fin) {
            _state = TCPState::State::CLOSE_WAIT;
            _sender.send_empty_segment();
            _receiver.stream_out().end_input();
            _linger_after_streams_finish = false;
        } else if (seg.payload().size() == 0) {
            if (seg.header().ackno.raw_value() <= _sender.next_seqno().raw_value() && !_sender.segments_out().size()) {
                return;
            }
        } else {
            _sender.send_empty_segment();
        }
    } else if (_state == TCPState::State::LISTEN) {
        if (seg.header().syn) {
            _state = TCPState::State::SYN_RCVD;
            send(true);
        }
        return;

    } else if (_state == TCPState::State::SYN_SENT) {
        if (seg.header().syn) {
            _state = TCPState::State::ESTABLISHED;
            _sender.send_empty_segment();
        }
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
    return data_size;  // bug
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!active()) {
        return;
    }
    _time_since_last_segment_received += ms_since_last_tick;
    if (_sender.bytes_in_flight()) {
        size_t s = _sender.consecutive_retransmissions();
        _sender.tick(ms_since_last_tick);
        if (_sender.consecutive_retransmissions() == s + 1) {
            send(false);
        }
    }
    if (_state == TCPState::State::TIME_WAIT && _time_since_last_segment_received >= _cfg.rt_timeout * 10) {
        if (!_sender.stream_in().eof()) {
            _sender.stream_in().end_input();
        }
        if (!_receiver.stream_out().eof()) {
            _receiver.stream_out().end_input();
        }
        _state = TCPState::State::CLOSED;
    }

    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown(true);
    }
}

void TCPConnection::end_input_stream()  // close
{
    _sender.stream_in().end_input();
    send(true);
    if (_state == TCPState::State::ESTABLISHED) {
        _state = TCPState::State::FIN_WAIT_1;
    } else if (_state == TCPState::State::CLOSE_WAIT) {
        _state = TCPState::State::LAST_ACK;
    } else  // error
    {
        unclean_shutdown(true);
        _state = TCPState::State::CLOSED;
    }
}

void TCPConnection::connect() {
    if (active() && _state != TCPState::State::LISTEN) {
        return;
    }
    send(true);
    _state = TCPState::State::SYN_SENT;
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            // Your code here: need to send a RST segment to the peer
            unclean_shutdown(true);
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
void TCPConnection::send(bool loading_window) {
    if (loading_window)
        _sender.fill_window();
    TCPSegment t;
    if (!_sender.segments_out().size()) {
        _sender.send_empty_segment_tmp();
    }
    while (_sender.segments_out().size()) {
        t = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value()) {
            t.header().win = _receiver.window_size();
            t.header().ack = 1;
            t.header().ackno = _receiver.ackno().value();
        }
        _segments_out.push(move(t));
    }
}
void TCPConnection::unclean_shutdown(bool is_send) {
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
    _linger_after_streams_finish = false;
    _state = TCPState::State::CLOSED;
    if (is_send) {
        TCPSegment t;
        t.header().rst = true;
        while (_segments_out.size()) {
            _segments_out.pop();
        }
        _segments_out.push(move(t));
    }
}