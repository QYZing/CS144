# 【计算机网络】cs144 lab

**为了更好的贯彻手拿TCP脚踩UDP兜里装个ICMP的意愿，遂写一个小lab**

由于再写时官网还没有更新完文档于是用了一个大佬搭的备份19年版本[【计算机网络】Stanford CS144 Lab Assignments 学习笔记 - 康宇PL - 博客园 (cnblogs.com)](https://www.cnblogs.com/kangyupl/p/stanford_cs144_labs.html)

###### 前言：

本lab实验环境在centos g++ 8 实现 cmake版本12

如果make 报错在tun.cc  可在里面添加头文件

```c++
#include "tun.hh"
#include "util.hh"
//下面这俩个
#include <sys/types.h>
#include <sys/socket.h>
```



## LAB0

###### Writing webget

利用提供的API写个GET请求就可以

```c++
void get_URL(const string &host, const string &path) {
    std::unique_ptr<TCPSocket> sock = std::make_unique<TCPSocket>();
    sock->connect(Address(host,"http"));
    std::string mesage  = "GET " + path + " HTTP/1.1\r\n"
                        +"Host: " + host + "\r\n"
                        +"Connection: close \r\n\r\n";
    sock->write(mesage);
    sock->shutdown(SHUT_WR);
    while(!sock->eof())
    {
        cout<<sock->read();
    }
    cerr << "Function called: get_URL(" << host << ", " << path << ").\n";
    cerr << "Warning: get_URL() has not been implemented yet.\n";
}
// ps  如果出现 t_webget (Failed)错误 修改tests/webget_t.sh 权限 +x
```

###### An in-memory reliable byte stream

要求实现一个有序字节流类（in-order byte stream），使之支持读写、容量控制。这个字节流类似于一个带容量的队列，从一头读，从另一头写。当流中的数据达到容量上限时，便无法再写入新的数据。特别的，写操作被分为了peek和pop两步。peek为从头部开始读取指定数量的字节，pop为弹出指定数量的字节。英语不好一开始还真看不太懂eof是干嘛用的

```c++
 //.hh
	size_t _capacity; 
    std::deque<char> _data {};
    size_t _bytes_Wrlen = 0 ;
    size_t _bytes_Poplen = 0;
    bool _input_signal = false;
    bool _error = false;  //!< Flag indicating that the stream suffered an error.
```

```c++
//.cc
ByteStream::ByteStream(const size_t capacity) :
 _capacity(capacity) {}

size_t ByteStream::write(const string &data1) {
    size_t Len  = remaining_capacity();
    size_t Add_length = Len >= data1.size() ? data1.size() : Len;
    for(size_t i =0 ;i < Add_length ; i++)
    {
        _data.push_back(data1[i]);
    }
    _bytes_Wrlen += Add_length;
    return Add_length;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t Len = _data.size();
    size_t Peek_Length = Len > len ? len : Len;
    return {_data.begin() ,_data.begin() + Peek_Length};
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t Len = _data.size();
    size_t Peek_Length = Len > len ? len : Len;
    _bytes_Poplen += Peek_Length;
    _data.erase(_data.begin() , _data.begin() + Peek_Length);
}

void ByteStream::end_input() { _input_signal = true;}

bool ByteStream::input_ended() const { return _input_signal; }

size_t ByteStream::buffer_size() const { return _data.size(); }

bool ByteStream::buffer_empty() const { return _data.empty(); }

bool ByteStream::eof() const { return _input_signal && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_Wrlen; }

size_t ByteStream::bytes_read() const { return _bytes_Poplen; }

size_t ByteStream::remaining_capacity() const {  return _capacity - _data.size(); }
```

## LAB1

本实验要求实现一个流重组器（stream reassembler）， 其中可以 重复， 乱序， 碎片

1 如果已经成功组成一个开头到之后的流片段那么应该立即放入_output中

2 EOF 可能提前到，也可能只是一个EOF空串

3 如果字节流满了同样无法写入，除非字节流被pop出，流重组器满了也一样，后续来的碎片只能被丢弃

根据本题碎片有可能交叉而且重复，且有EOF限制

```
根据上述分析 来的是碎片化的字节流 而且有重复， 就相当于一个一个集合，当集合有交集的时候可以合成一个大集合
不过我打算直接用vector<char> 来存储 因为只有开头流是一个整体就可以存入byte_stream所以维护一个已存入流的下标就可以了，另外就是区分碎片和非碎片，我选用了vector<bool> 来进行辨别（vecotr<bool> 遍历时不用引用修改）, 碎片有交集合并这件事就很简单了，因为本身数组特性就可以去重合并，其中当开头插入一个碎片时还需判断后续碎片是否已经完整，所以记录一个下标判断到不完整即可 而且由于防止byte不能同步写入记录一个下标用于记录写入数量
```

###### stream_reassembler.hh

```c++
  private: 
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity ;      //!< The maximum number of bytes
    // Your code here -- add private members as necessary.
    std::vector<char> _assembles {} ; //assembles string by index  if ok -> _output
    std::vector<bool> _set{}; // ture or false exist
    size_t _now_index {}; // assembled bytes index
    size_t _now_assem_byteix {}; //alreday write _output index
    size_t _end_index =  -1;  // eof index 
    size_t _sum_bytes {}; // 每一个窗口写入的字节数
    int _full_flag {}; // 当容器满了但是还eof还没到++
```

###### stream_reassembler.cc

```c++
StreamReassembler::StreamReassembler(const size_t capacity) :
                     _output(capacity), _capacity(capacity) {
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
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) 
{
    size_t offset = _full_flag * _capacity;
    size_t end = data.size() + index;
    size_t start = index >  _now_index + offset ? index : _now_index + offset;
    if(end  - offset > _capacity) end = _capacity + offset;
    for(size_t i = start ; i < end ; i++)
    {
        size_t ci = i % (_capacity);
        if(_set[ci] == 0)
        {
            _set[ci] = 1;
            _assembles[ci] = data[i - index];
            ++_sum_bytes;
        }
        if(_now_index == ci)
        {
            _now_index =end  - offset ;
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
        {
            datatmp += _assembles[i];
            _set[i] = 0;
        } 
	    size_t size = _output.write(datatmp);
         _now_assem_byteix += size; // ever there have bug
    }
    if(1 == eof) _end_index = end;
    if(_end_index  == _now_index + offset)
    {
        _output.end_input();
    }
    else if(_now_assem_byteix == _capacity)
    {
         _output.end_input();
        _full_flag++;
        _now_index = 0;
        _sum_bytes = 0;
        _now_assem_byteix = 0;
    }
}
size_t StreamReassembler::unassembled_bytes() const {
   return _sum_bytes - _now_index;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
```

## LAB2

  本文要求实现一个处理Tcp连接请求接收消息的 TCP Receiver（ps） 由于我上面的写法是只管理了一个窗口于是在这节疯狂打补丁形成滑动窗口 （就是一个窗口装不下的时候，先缓存一下，然后在装）

###### stream_reassembler.cc 打补丁

```c++
//增加了俩个函数
size_t StreamReassembler::residue() const //该窗口剩余容量
{
    return _capacity - _now_assem_byteix;
}

uint64_t  StreamReassembler::headno() const //int64真实头长度
{
    return _full_flag * _capacity + _now_assem_byteix;
}
```

###### tcp_receiver.hh

```c++
// 记录一下isn 
private:  
	StreamReassembler _reassembler;
    //! The maximum number of bytes we'll store.
    size_t _capacity;

    uint64_t _isn{0};
    int32_t _start_flag {};//模拟uint64长度
    bool _eof{};
    int _null_offset {};//fin 和 syn 有没有同时出现
```

###### tcp_receiver.cc

```c++
//syn 和 fin 同时来就 seq+2  并且他们来都可能带数据所以先处理标志然后处理数据
bool TCPReceiver::segment_received(const TCPSegment &seg) {
    int ret = 0; // syn and fin can possible  come here thar at the same time
    uint64_t index {}; // push_string index 
    if( 1 ==  seg.header().syn)//deal syn
    {  
        if(1 == _start_flag) return false; 
        _start_flag = 1;
        _isn = seg.header().seqno.raw_value();   
        ret++;
         _null_offset++;
        if(seg.length_in_sequence_space() == 1) return true; //only syn
    }
    else 
    {    
        if(0 == _start_flag ) return false;  
        // @don`t have syn   then  deal flow push_string index
        index =unwrap(seg.header().seqno , WrappingInt32(_isn) , _start_flag * _capacity) -1;
       // cout<<" -- index "<<index<<endl;
    }
    if(1 == seg.header().fin)//deal fin  
    {  
        if(1 == _eof) return false;     
         ret++;
         _null_offset++;
        _eof = 1;
    }
    else 
    {
        uint64_t top = (_reassembler.headno() - _reassembler.headno() % _capacity )+ _capacity;
        uint64_t low = _reassembler.headno();
        if(index >=  top || index + seg.length_in_sequence_space() - ret <= low)
            return false; // not in window
    }
    //@ deal byte flow 
    //@ imitate sliding window at twice puhs_string  
    size_t redata_size  = seg.payload().size(); // virtual size 
    size_t now_remasmbler_size = _reassembler.residue(); // reality size   
    if(now_remasmbler_size < redata_size)//他的字节数比我虚报的空间大，分两次发
    {
        string data_tmp = seg.payload().copy();
        _reassembler.push_substring(data_tmp.substr(0,now_remasmbler_size),index,0);
        _reassembler.push_substring(data_tmp.substr(now_remasmbler_size),index + now_remasmbler_size,0);
    }
    else
    {
         _reassembler.push_substring(seg.payload().copy(), index , seg.header().fin);
    } 
     if(1 == _reassembler.stream_out().input_ended() ) _start_flag++;
    return true;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if(_start_flag)
        return { wrap(_reassembler.headno() + _null_offset ,WrappingInt32(_isn)) };
    return  std::nullopt;
 }

size_t TCPReceiver::window_size() const //virtual size
{ return _capacity - _reassembler.stream_out().buffer_size(); }


```





