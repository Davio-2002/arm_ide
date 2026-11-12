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
// Address of each line-table row → line in sourcePath, 0 for code outside it.
std::map<uint64_t, int> readSourceLines( const std::string &executablePath,
                                         const std::string &sourcePath );
// 0 when the executable does not define it.
uint64_t symbolAddress( const std::string &executablePath, const std::string &name );
