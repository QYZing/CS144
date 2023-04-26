<<<<<<< HEAD
For build prereqs, see [the CS144 VM setup instructions](https://web.stanford.edu/class/cs144/vm_howto).

## Sponge quickstart

To set up your build directory:

	$ mkdir -p <path/to/sponge>/build
	$ cd <path/to/sponge>/build
	$ cmake ..

**Note:** all further commands listed below should be run from the `build` dir.

To build:

    $ make

You can use the `-j` switch to build in parallel, e.g.,

    $ make -j$(nproc)

To test (after building; make sure you've got the [build prereqs](https://web.stanford.edu/class/cs144/vm_howto) installed!)

    $ make check

The first time you run `make check`, it will run `sudo` to configure two
[TUN](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) devices for use during
testing.

### build options

You can specify a different compiler when you run cmake:

    $ CC=clang CXX=clang++ cmake ..

You can also specify `CLANG_TIDY=` or `CLANG_FORMAT=` (see "other useful targets", below).

Sponge's build system supports several different build targets. By default, cmake chooses the `Release`
target, which enables the usual optimizations. The `Debug` target enables debugging and reduces the
level of optimization. To choose the `Debug` target:

    $ cmake .. -DCMAKE_BUILD_TYPE=Debug

The following targets are supported:

- `Release` - optimizations
- `Debug` - debug symbols and `-Og`
- `RelASan` - release build with [ASan](https://en.wikipedia.org/wiki/AddressSanitizer) and
  [UBSan](https://developers.redhat.com/blog/2014/10/16/gcc-undefined-behavior-sanitizer-ubsan/)
- `RelTSan` - release build with
  [ThreadSan](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Thread_Sanitizer)
- `DebugASan` - debug build with ASan and UBSan
- `DebugTSan` - debug build with ThreadSan

Of course, you can combine all of the above, e.g.,

    $ CLANG_TIDY=clang-tidy-6.0 CXX=clang++-6.0 .. -DCMAKE_BUILD_TYPE=Debug

**Note:** if you want to change `CC`, `CXX`, `CLANG_TIDY`, or `CLANG_FORMAT`, you need to remove
`build/CMakeCache.txt` and re-run cmake. (This isn't necessary for `CMAKE_BUILD_TYPE`.)

### other useful targets

To generate documentation (you'll need `doxygen`; output will be in `build/doc/`):

    $ make doc

To lint (you'll need `clang-tidy`):

    $ make -j$(nproc) tidy

To run cppcheck (you'll need `cppcheck`):

    $ make cppcheck

To format (you'll need `clang-format`):

    $ make format

To see all available targets,

    $ make help
=======
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
不过我打算直接用vector<char> 来存储 因为只有开头流是一个整体就可以存入byte_stream所以维护一个已存入流的下标就可以了，另外就是区分碎片和非碎片，我选用了vector<bool> 来进行辨别（vecotr<bool> 遍历时不用引用修改）, 碎片有交集合并这件事就很简单了，因为本身数组特性就可以去重合并，其中当开头插入一个碎片时还需判断后续碎片是否已经完整，所以记录一个下标判断到不完整即可
```

###### stream_reassembler.hh

```c++
  private: 
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity ;    //!< The maximum number of bytes
    // Your code here -- add private members as necessary.
    std::vector<char> _assembles {} ; // assembles string by index  if ok -> _output
    std::vector<bool> _set{}; // ture or false   on  exist
    size_t _now_index {}; // assembled bytes index
    size_t _now_assem_byteix {}; //alreday write _output index 这个和上个其实有一个就行
    size_t _end_index ;  // eof index  边界
    size_t _sum_bytes {};
```

###### stream_reassembler.cc

```c++

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
    //稍微优化一点，对比其他人用set， priority_queue实现，感觉性能还可以
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
        _output.end_input();//EOF标志
    }
}
size_t StreamReassembler::unassembled_bytes() const {
   return _sum_bytes - _now_index;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }

```













>>>>>>> 2d7953ebe1acb0599821e52d7f5b02f54d829419
