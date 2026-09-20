#include "md5.hpp"
#include <boost/iostreams/device/mapped_file.hpp>
#include <filesystem>
#include <iostream>
#include <print>
#include <span>
#include <string_view>

auto main([[maybe_unused]] const int argc, [[maybe_unused]] const char* const* const argv) -> int
{
    const std::span ARGUMENTS {argv, static_cast<std::size_t>(argc)};

    if (ARGUMENTS.size() < 2 || std::string_view(ARGUMENTS.at(1)) == "-")
    {
        // Read all bytes from stdin into a vector
        // (Stdind streams aren't seekable, so we buffer the stream for the range-based md5)
        std::vector<std::byte> buffer;
        char                   chr {};
        while (std::cin.get(chr))
        {
            buffer.push_back(static_cast<std::byte>(chr));
        }

        const auto DIGEST = md5(buffer);
        std::println("{}  -", digestToString(DIGEST));
        return 0;
    }

    for (const auto* const ptr_cstrFilePath : ARGUMENTS | std::views::drop(1))
    {
        std::filesystem::path path {std::string_view(ptr_cstrFilePath)};

        if (!std::filesystem::exists(path))
        {
            std::println(std::cerr, "File does not exist: {}", path);
            continue;
        }
        if (!std::filesystem::is_regular_file(path))
        {
            std::println(std::cerr, "File is not a regular file: {}", path);
            continue;
        }

        boost::iostreams::mapped_file_source file {path};
        auto       byteView = file | std::views::transform([](const char CHR) { return static_cast<std::byte>(CHR); });
        const auto DIGEST   = md5(byteView);
        std::println("{}  {}", digestToString(DIGEST), path);
    }
    return 0;
}
