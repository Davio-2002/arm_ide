#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <core/gdb_remote.h>

namespace {

/// @return True if all bytes were sent.
bool sendRaw( GdbConnection &connection, const std::string &data ) {
    size_t sent = 0;
    while ( sent < data.size() ) {
        ssize_t count =
            send( connection.socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL );
        if ( count <= 0 ) {
            return false;
        }
        sent += count;
    }
    return true;
}

/// @brief Takes one complete packet from the received bytes and acknowledges it.
/// @return True if a packet was taken.
bool takePacket( GdbConnection &connection, std::string &payload ) {
    size_t start = connection.received.find( '$' );
    if ( start == std::string::npos ) {
        connection.received.clear();
        return false;
    }
    size_t end = connection.received.find( '#', start );
    if ( end == std::string::npos || end + 2 >= connection.received.size() ) {
        connection.received.erase( 0, start );
        return false;
    }
    payload = connection.received.substr( start + 1, end - start - 1 );
    connection.received.erase( 0, end + 3 );
    sendRaw( connection, "+" );
    return true;
}

} // anonymous namespace

std::string toHex( uint64_t value ) {
    char text[17];
    std::snprintf( text, sizeof( text ), "%llx", static_cast<unsigned long long>( value ) );
    return text;
}

uint64_t parseHex( const std::string &text ) {
    return std::strtoull( text.c_str(), nullptr, 16 );
}

uint64_t littleEndian( const std::string &hex, size_t offset, size_t byteCount ) {
    uint64_t value = 0;
    for ( size_t i = 0; i < byteCount; i++ ) {
        value |= parseHex( hex.substr( offset + i * 2, 2 ) ) << ( i * 8 );
    }
    return value;
}

bool isValidSocketPath( const std::string &path ) {
    return path.size() < sizeof( sockaddr_un::sun_path );
}

bool connectGdb( GdbConnection &connection, const std::string &path ) {
    sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    std::copy( path.begin(), path.end(), address.sun_path );

    connection.socket = socket( AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0 );
    if ( connect( connection.socket, reinterpret_cast<sockaddr *>( &address ),
                  sizeof( address ) ) == 0 ) {
        return true;
    }
    closeGdb( connection );
    return false;
}

void closeGdb( GdbConnection &connection ) {
    if ( connection.socket >= 0 ) {
        close( connection.socket );
    }
    connection = GdbConnection();
}

bool sendGdbPacket( GdbConnection &connection, const std::string &payload ) {
    unsigned checksum = 0;
    for ( char c : payload ) {
        checksum += static_cast<unsigned char>( c );
    }
    char end[4];
    std::snprintf( end, sizeof( end ), "#%02x", checksum & 0xff );
    return sendRaw( connection, "$" + payload + end );
}

GdbReceive receiveGdbPacket( GdbConnection &connection, std::string &payload,
                             int timeoutMilliseconds ) {
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds( timeoutMilliseconds );
    while ( !takePacket( connection, payload ) ) {
        auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now() );
        pollfd descriptor = { connection.socket, POLLIN, 0 };
        if ( poll( &descriptor, 1, std::max<int>( 0, left.count() ) ) <= 0 ) {
            return GdbReceive::TimedOut;
        }
        char buffer[4096];
        ssize_t count = recv( connection.socket, buffer, sizeof( buffer ), 0 );
        if ( count <= 0 ) {
            return GdbReceive::Closed;
        }
        connection.received.append( buffer, count );
    }
    return GdbReceive::Packet;
}

bool gdbRequest( GdbConnection &connection, const std::string &payload, std::string &reply,
                 int timeoutMilliseconds ) {
    return connection.socket >= 0 && sendGdbPacket( connection, payload ) &&
           receiveGdbPacket( connection, reply, timeoutMilliseconds ) == GdbReceive::Packet;
}
