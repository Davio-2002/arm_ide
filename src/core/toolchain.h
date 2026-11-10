#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

enum class MessageKind { Output, Command, Error, Warning, Success };

struct Message {
    std::string text;
    MessageKind kind = MessageKind::Output;
    int sourceLine = 0;
};

struct BuildResult {
    bool success = false;
    std::vector<Message> messages;
    std::string executablePath;
};

BuildResult buildProgram( const std::string &sourcePath );
std::map<uint64_t, int> readSourceLines( const std::string &executablePath );
