#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    EthernetFrame frame;
    frame.payload().append(dgram.serialize());
    frame.header().src = _ethernet_address;
    frame.header().type = EthernetHeader::TYPE_IPv4;

    if(_ip_mac.count(next_hop_ip) == 0 || _ip_mac[next_hop_ip]._ttl  +  30000 <= _now_time){
        _frame_wait[next_hop_ip].push_back(frame);
        if(_frame_wait[next_hop_ip].size() > 1 && _last_time[next_hop_ip] + 5000 >= _now_time) return ;
        EthernetFrame _look_for;
        _look_for.header().src = _ethernet_address;
        _look_for.header().type = EthernetHeader::TYPE_ARP;
        _look_for.header().dst = ETHERNET_BROADCAST;
        ARPMessage mes;
        mes.opcode = ARPMessage::OPCODE_REQUEST;
        mes.sender_ethernet_address = _ethernet_address;
        mes.sender_ip_address = _ip_address.ipv4_numeric();
        mes.target_ip_address =  next_hop_ip ;

        _look_for.payload().append(mes.serialize());
        _last_time[next_hop_ip] = _now_time;
        _frames_out.push(move(_look_for));

    }else {
        frame.header().dst = _ip_mac[next_hop_ip]._mac;
        _frames_out.push(move(frame));
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    cerr << (frame.header().dst == ETHERNET_BROADCAST) <<"   "<<(frame.header().dst == _ethernet_address) <<"\n";
    if(frame.header().dst != _ethernet_address && frame.header().dst != ETHERNET_BROADCAST )
        return std::nullopt;
    if(frame.header().type == EthernetHeader::TYPE_IPv4)
    {
        cerr <<" im come here ipv4 "<<endl;
        InternetDatagram data;
        data.parse(frame.payload());
        //if(data.header().dst != _ip_address.ipv4_numeric())
            //return std::nullopt;
        return data;
    }
    else if(frame.header().type == EthernetHeader::TYPE_ARP )
    {
        cerr <<" im come here arp "<<endl;
        ARPMessage mes;
        mes.parse(frame.payload());
        if(mes.target_ip_address != _ip_address.ipv4_numeric()) return std::nullopt;
        MemoryEthAddr m_eth;
        m_eth._mac = mes.sender_ethernet_address;
        m_eth._ttl = _now_time;
        _ip_mac[mes.sender_ip_address] = m_eth;
        if(frame.header().dst == ETHERNET_BROADCAST  )
        {
            ARPMessage send_mes;
            send_mes.opcode = ARPMessage::OPCODE_REPLY;
            send_mes.sender_ethernet_address = _ethernet_address;
            send_mes.sender_ip_address = _ip_address.ipv4_numeric();
            send_mes.target_ip_address =  mes.sender_ip_address; 
            send_mes.target_ethernet_address = mes.sender_ethernet_address;
            EthernetFrame frm;
            frm.payload().append(send_mes.serialize());
            frm.header().src = _ethernet_address;
            frm.header().dst = mes.sender_ethernet_address;
            frm.header().type = EthernetHeader::TYPE_ARP;
            _frames_out.push(move(frm));
        }
        if(_frame_wait.count(mes.sender_ip_address) != 0)
        {
            for(EthernetFrame &frm : _frame_wait[mes.sender_ip_address] )
            {
                frm.header().dst = frame.header().src;
                _frames_out.push(frm);
            }
            _frame_wait[mes.sender_ip_address].clear();
        }
    }
    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) { 
    _now_time += ms_since_last_tick;
}

