#include "md5.hpp"
#include <exception>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnull-dereference"
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/asio/use_future.hpp>
#pragma GCC diagnostic pop

#include <boost/iostreams/device/mapped_file.hpp>
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

        const auto DIGEST = md5(buffer);
        std::println("{}  -", digestToString(DIGEST));
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
        constexpr static auto HAS_READ_PERMISSION = [](const std::filesystem::path& path)
        { return ::faccessat(AT_FDCWD, path.c_str(), R_OK, AT_EACCESS) == 0; };
        if (!HAS_READ_PERMISSION(path))
        {
            return std::unexpected<std::string> {"File is not readable: " + path.display_string()};
        }

        try
        {
            boost::iostreams::mapped_file_source file {path};
            const auto BYTE_VIEW = file | std::views::transform([](const char CHR) { return static_cast<std::byte>(CHR); });
            const auto DIGEST = md5(BYTE_VIEW);
            return digestToString(DIGEST) + "  " + std::string(path);
        }
        catch (const std::exception& ex)
        {
            return std::unexpected<std::string> {"Failed to open file: " + path.display_string() + ": " + ex.what()};
        }
    };

    if (ARGUMENTS.size() == 2)
    {
        const auto RESULT = work(std::filesystem::path(ARGUMENTS.at(1)));
        if (RESULT)
        {
            std::println("{}", RESULT.value());
        }
        else
        {
            std::println("{}", RESULT.error());
        }
        return 0;
    }

    const auto               THREAD_COUNT = std::thread::hardware_concurrency();
    boost::asio::thread_pool pool {THREAD_COUNT > 0 ? THREAD_COUNT : 4};

    std::vector<std::future<std::expected<std::string, std::string>>> futures;

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
        auto result = fut.get();    // Blocks until this specific file is done
        if (result)
        {
            std::cout << *result << "\n";
        }
        else
        {
            std::cerr << result.error() << "\n";
        }
    }

    pool.join();    // Wait for all threads to wrap up

    return 0;
}
