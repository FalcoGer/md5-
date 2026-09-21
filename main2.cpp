#include "md5.hpp"
#include <algorithm>
#include <exception>
#include <fstream>
#include <inplace_vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>
#pragma GCC diagnostic pop

#include <expected>
#include <fcntl.h>    // For AT_FDCWD and AT_EACCESS
#include <filesystem>
#include <future>
#include <iostream>
#include <print>
#include <span>
#include <string_view>
#include <unistd.h>    // For faccessat

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

        const auto DIGEST = MD5::md5(buffer);
        std::println("{}  -", DIGEST.toString());
        return 0;
    }

    auto work = [](const std::filesystem::path& path) -> std::expected<std::string, std::string>
    {
        if (!std::filesystem::exists(path))
        {
            return std::unexpected<std::string> {"File does not exist: " + path.display_string()};
        }
        if (!std::filesystem::is_regular_file(path))
        {
            return std::unexpected<std::string> {"File is not a regular file: " + path.display_string()};
        }

        const bool HAS_READ_PERMISSION = ::faccessat(AT_FDCWD, path.c_str(), R_OK, AT_EACCESS) == 0;
        if (!HAS_READ_PERMISSION)
        {
            return std::unexpected<std::string> {"File is not readable: " + path.display_string()};
        }

        try
        {
            auto file = std::ifstream(path, std::ios_base::binary);
            if (!file.is_open())
            {
                return std::unexpected<std::string> {"Failed to open file: " + path.display_string()};
            }

            MD5::Digest                                        digest;

            // read chunks of 64 into a vector of bytes until remaining size is less than 64 or end of file
            std::inplace_vector<std::byte, MD5::Chunk::size()> buffer;
            std::array<std::byte, MD5::Chunk::size()>          data {};
            std::streamsize                                    bytesRead {};
            std::size_t                                        totalBytesRead {};
            while (true)
            {
                buffer.clear();
                // read 64 byte at once
                file.read(reinterpret_cast<char*>(data.data()), MD5::Chunk::size());
                bytesRead       = file.gcount();
                totalBytesRead += static_cast<std::size_t>(bytesRead);
                std::ranges::copy_n(data.begin(), bytesRead, std::back_inserter(buffer));
                if (buffer.size() < MD5::Chunk::size())
                {
                    break;
                }

                auto it = std::begin(buffer);
                digest.processChunk(MD5::Chunk(it));
            }
            // process the rest of the file
            digest.digestTail(buffer, totalBytesRead);

            return digest.toString() + "  " + std::string(path);
        }
        catch (const std::exception& ex)
        {
            return std::unexpected<std::string> {"Failed to open file: " + path.display_string() + ": " + ex.what()};
        }
    };

    if (ARGUMENTS.size() == 2)
    {
        const auto RESULT = work(std::filesystem::path(ARGUMENTS.at(1)));
        std::ignore       = RESULT.transform([](const std::string& str) { std::println("{}", str); })
                              .transform_error(
                                [](const std::string& str)
                                {
                              std::println(std::cerr, "{}", str);
                              return 1;
                                }
                              );

        return 0;
    }

    const auto               THREAD_COUNT = std::thread::hardware_concurrency();
    boost::asio::thread_pool pool {THREAD_COUNT > 0 ? THREAD_COUNT : 4};

    std::vector<std::future<std::expected<std::string, std::string>>> futures;
    futures.reserve(ARGUMENTS.size() - 1);

    for (const auto* const ptr_cstrFilePath : ARGUMENTS | std::views::drop(1))
    {
        const std::filesystem::path FILE_PATH {ptr_cstrFilePath};

        futures.push_back(
          boost::asio::post(pool, boost::asio::use_future([work, FILE_PATH] { return work(FILE_PATH); }))
        );
    }

    // Print results in strict argument order
    for (auto& fut : futures)
    {
        const auto RESULT = fut.get();
        std::ignore       = RESULT.transform([](const std::string& str) { std::println("{}", str); })
                              .transform_error(
                                [](const std::string& str)
                                {
                              std::println(std::cerr, "{}", str);
                              return 1;
                                }
                              );
    }

    pool.join();    // Wait for all threads to wrap up

    return 0;
}
