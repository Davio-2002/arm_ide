#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/// @brief GDB Remote Serial Protocol connection over a Unix socket.
struct GdbConnection {
    int socket = -1;
    std::string received;
};

enum class GdbReceive { Packet, TimedOut, Closed };

/// @return Lower-case hex digits without prefix.
std::string toHex( uint64_t value );

/// @param[in] text Hex digits without prefix.
/// @return Parsed number; 0 for invalid text.
uint64_t parseHex( const std::string &text );

/// @brief Decodes a little-endian number, two hex digits per byte.
/// @param[in] hex       Register or memory reply.
/// @param[in] offset    Index of the first digit.
/// @param[in] byteCount Bytes to decode.
/// @return Decoded number.
uint64_t littleEndian( const std::string &hex, size_t offset, size_t byteCount );

/// @brief Encodes a 64-bit register value as a `P` packet expects it.
/// @return 16 hex digits, least significant byte first.
std::string littleEndianHex( uint64_t value );

/// @return True if the path fits in a Unix socket address.
bool isValidSocketPath( const std::string &path );

/// @brief Tries once to connect.
/// @param[in,out] connection Closed connection; gets the socket.
/// @param[in]     path       Socket path, valid by isValidSocketPath().
/// @return True if connected.
bool connectGdb( GdbConnection &connection, const std::string &path );

/// @brief Closes the socket and drops received bytes.
void closeGdb( GdbConnection &connection );

/// @brief Sends a packet with its checksum.
/// @param[in] payload Command, e.g. "g" or "m1000,4".
/// @return True if sent.
bool sendGdbPacket( GdbConnection &connection, const std::string &payload );

/// @brief Waits for the next packet and acknowledges it.
/// @param[out] payload             Payload without "$" and checksum.
/// @param[in]  timeoutMilliseconds Longest wait.
/// @return Packet, TimedOut, or Closed by the stub.
GdbReceive receiveGdbPacket( GdbConnection &connection, std::string &payload,
                             int timeoutMilliseconds );

/// @brief Sends a command and waits for its reply.
/// @param[out] reply               Reply payload.
/// @param[in]  timeoutMilliseconds Longest wait.
/// @return True if a reply arrived.
bool gdbRequest( GdbConnection &connection, const std::string &payload, std::string &reply,
                 int timeoutMilliseconds );
