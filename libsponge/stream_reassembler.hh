#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <string>
#include <vector>
//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private: 
    ByteStream _output;  //!< The reassembled in-order byte stream
    size_t _capacity ;      //!< The maximum number of bytes
    // Your code here -- add private members as necessary.
    std::vector<char> _assembles {} ; //******???????? assembles string by index  if ok -> _output
    std::vector<bool> _set{}; // ture or false exist
    size_t _now_index {}; // assembled bytes index
    size_t _now_assem_byteix {}; //alreday write _output index
    size_t _end_index =  -1;  // eof index 
    size_t _sum_bytes {};
    int _full_flag {}; // 当容器满了但是还eof还没到++
  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receives a substring and writes any newly contiguous bytes into the stream.
    //!
    //! If accepting all the data would overflow the `capacity` of this
    //! `StreamReassembler`, then only the part of the data that fits will be
    //! accepted. If the substring is only partially accepted, then the `eof`
    //! will be disregarded.
    //!
    //! \param data the string being added
    //! \param index the index of the first byte in `data`¼
    //! \param eof whether or not this segment ends with the end of the stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been submitted twice, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    // add residue windows
    size_t residue() const;

    uint64_t headno() const ;
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
