#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

/// @brief A connection to a GDB Remote Serial Protocol stub over a Unix socket.
///
/// Packets are "$payload#xx" with a two-digit checksum; each received packet is acknowledged
/// with "+".
struct GdbConnection {
    int socket = -1;
    std::string received;
};

/// @brief Result of receiveGdbPacket().
enum class GdbReceive { Packet, TimedOut, Closed };

/// @brief Formats a number as lower-case hex without a prefix, as the protocol expects.
/// @return Hex digits.
std::string toHex( uint64_t value );

/// @brief Parses hex digits without a prefix.
/// @param[in] text Hex digits.
/// @return Parsed number; 0 for invalid text.
uint64_t parseHex( const std::string &text );

/// @brief Decodes a little-endian number from a hex string, two digits per byte.
/// @param[in] hex       Hex string, as in register and memory replies.
/// @param[in] offset    Index of the first hex digit.
/// @param[in] byteCount Number of bytes to decode.
/// @return Decoded number.
uint64_t littleEndian( const std::string &hex, size_t offset, size_t byteCount );

/// @param[in] path Socket path.
/// @return True if the path fits in a Unix socket address.
bool isValidSocketPath( const std::string &path );

/// @brief Tries once to connect to the stub's socket.
/// @param[in,out] connection Closed connection; gets the socket on success.
/// @param[in]     path       Socket path; must pass isValidSocketPath().
/// @return True if connected.
bool connectGdb( GdbConnection &connection, const std::string &path );

/// @brief Closes the socket and drops received bytes.
/// @param[in,out] connection Connection to close.
void closeGdb( GdbConnection &connection );

/// @brief Sends a packet with its checksum.
/// @param[in]     payload    Command, for example "g" or "m1000,4".
/// @return True if the packet was sent.
bool sendGdbPacket( GdbConnection &connection, const std::string &payload );

/// @brief Waits for the next packet and acknowledges it.
/// @param[out]    payload             Packet payload, without "$" and the checksum.
/// @param[in]     timeoutMilliseconds Longest time to wait.
/// @return Packet if one arrived, TimedOut, or Closed if the stub closed the connection.
GdbReceive receiveGdbPacket( GdbConnection &connection, std::string &payload,
                             int timeoutMilliseconds );

/// @brief Sends a command and waits for its reply.
/// @param[out]    reply               Reply payload.
/// @param[in]     timeoutMilliseconds Longest time to wait for the reply.
/// @return True if a reply arrived.
bool gdbRequest( GdbConnection &connection, const std::string &payload, std::string &reply,
                 int timeoutMilliseconds );
