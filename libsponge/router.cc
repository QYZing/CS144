#include "router.hh"

#include <iostream>
#include <bitset>
using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    _rt_mes.push_back({route_prefix,prefix_length , next_hop ,interface_num });
    // Your code here.
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    string ips = bitset<32>(dgram.header().dst).to_string();
    uint8_t max_length = 0;
    size_t index = 0;
    int flag = true;
    for(size_t p =0 ;p < _rt_mes.size() ; p++){
        auto & i = _rt_mes[p];
        string t =bitset<32>(i.route_prefix).to_string();
        if(t.substr(0 , i.prefix_length) == ips.substr(0 , i.prefix_length))
        {
            if(i.prefix_length > max_length || flag)
            {
                max_length = i.prefix_length;
                index = p;
                flag = false;
            }
        }
    }
    if(flag) return ;
    if(dgram.header().ttl <= 1)
        return ;
    dgram.header().ttl--;
    if(_rt_mes[index].next_hop.has_value())
    {
        interface(_rt_mes[index].interface_num).send_datagram(dgram , _rt_mes[index].next_hop.value());
    }
    else
    {
        interface(_rt_mes[index].interface_num).send_datagram(dgram , Address::from_ipv4_numeric(dgram.header().dst));
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
    }
}
