#pragma once

#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gcem.hpp>
#include <iterator>
#include <ranges>
#include <span>
#include <string>
#include <string_view>

static constexpr std::size_t MD5_DIGEST_LEN = 16;

// TODO: Test in big endian architectures

static_assert(
  std::endian::native == std::endian::little || std::endian::native == std::endian::big,
  "Native endianness not supported"
);

[[nodiscard]]
constexpr auto md5(const std::ranges::range auto& message) -> std::array<std::byte, MD5_DIGEST_LEN>
    requires std::same_as<std::ranges::range_value_t<decltype(message)>, std::byte>
{
    static constexpr std::size_t                                  TOTAL_ROUND_COUNT = 64;
    static constexpr std::size_t                                  CHUNK_SIZE        = 64;
    // shift amounts for every idx-th round
    static constexpr std::array<std::uint32_t, TOTAL_ROUND_COUNT> S                 = {
      {
       7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
       14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
       4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
       }
    };

    static constexpr std::array<std::uint32_t, TOTAL_ROUND_COUNT> K = []
    {
        std::array<std::uint32_t, TOTAL_ROUND_COUNT> result {};
        for (auto&& [idx, val] : result | std::ranges::views::enumerate)
        {
            //NOLINTNEXTLINE(readability-magic-numbers)
            val = static_cast<std::uint32_t>(gcem::floor(gcem::pow(2.0, 32.0) * gcem::abs(gcem::sin(idx + 1))));
        }
        return result;
    }();

    static constexpr std::array<std::uint32_t, MD5_DIGEST_LEN / sizeof(std::uint32_t)> DIGEST_INIT {
      {
       0x67'45'23'01,    // A
        0xef'cd'ab'89,    // B
        0x98'ba'dc'fe,    // C
        0x10'32'54'76,    // D
      }
    };
    auto                digest                 = DIGEST_INIT;


    // prepare the message by
    // 1. Unconditionally appending a single 1 bit (for whole byte messages we append 0x80)
    // 2. Then append zeroes until message bit length is 448 bits mod 512 (or 56 bytes mod 64)
    // 3. Then append original message length in bits mod 64
    // This creates a message that can be chunked into 512 bits (64 bytes)

    const std::size_t   TOTAL_BYTES            = std::size(message);
    const std::uint64_t TOTAL_BITS             = std::uint64_t {TOTAL_BYTES}
                                                 * std::uint64_t {8};    // NOLINT(readability-magic-numbers)
    const auto          MESSAGE_LEN_INFO_BYTES = [](std::uint64_t len)
    {
        if constexpr (std::endian::native == std::endian::big)
        {
            len = std::byteswap(len);
        }
        return std::bit_cast<std::array<std::byte, sizeof(std::uint64_t)>>(len);
    }(TOTAL_BITS);

    // lets find out how many zeroes we need to append first
    // then use a concat view to lazily evaluate during execution

    // + chunk size and mod chunk size again to deal with cases where 56 - message size + 1 is negative.
    const auto ZEROES_COUNT = (CHUNK_SIZE - sizeof(TOTAL_BITS) - ((message.size() + 1) % CHUNK_SIZE) + CHUNK_SIZE)
                              % CHUNK_SIZE;

    const auto PADDING_VIEW = std::views::concat(
      std::views::single(std::byte {0x80}), std::views::repeat(std::byte {0x00}, ZEROES_COUNT), MESSAGE_LEN_INFO_BYTES
    );

    auto processChunk = [&digest](auto chunkIt)
    {
        // we break each chunk into 16 * 32 bit words, labeled M[0 .. 15]
        const std::array<std::uint32_t, CHUNK_SIZE / sizeof(std::uint32_t)> M = [&chunkIt]
        {
            std::array<std::uint32_t, CHUNK_SIZE / sizeof(std::uint32_t)> words {};
            if constexpr (std::endian::native == std::endian::little && std::contiguous_iterator<decltype(chunkIt)>)
            {
                if constexpr (std::contiguous_iterator<decltype(chunkIt)>)
                {
                    std::memcpy(words.data(), std::to_address(chunkIt), CHUNK_SIZE);
                }
                else
                {
                    // std::as_writable_bytes is not constexpr
                    auto bytes = std::as_writable_bytes(std::span {words});
                    std::ranges::copy_n(chunkIt, CHUNK_SIZE, bytes.begin());
                }
            }
            else
            {
                // Big endian or not consteval
                auto it = chunkIt;
                for (unsigned int& word : words)
                {
                    // NOLINTBEGIN(readability-magic-numbers)
                    word = static_cast<std::uint32_t>(*it++) << 0U;
                    word |= static_cast<std::uint32_t>(*it++) << 8U;
                    word |= static_cast<std::uint32_t>(*it++) << 16U;
                    word |= static_cast<std::uint32_t>(*it++) << 24U;
                    // NOLINTEND(readability-magic-numbers)
                }
            }
            return words;
        }();

        // Init hash values for this chunk
        auto [a, b, c, d] = digest;    // NOLINT(readability-identifier-length)

        // Run the Md5 hashing algorithm main loop (64 times)
        for (const std::uint32_t CURRENT_ROUND :
             std::ranges::views::iota(std::uint32_t {0}, static_cast<std::uint32_t>(TOTAL_ROUND_COUNT)))
        {
            struct FG
            {
                std::uint32_t m_f;
                std::uint32_t m_g;
            };
            auto [f, g] = [&] -> FG    // NOLINT(readability-identifier-length)
            {
                if (CURRENT_ROUND < TOTAL_ROUND_COUNT / 4U)
                {
                    return {(b & c) | ((~b) & d), CURRENT_ROUND};
                }
                if (CURRENT_ROUND < 2U * TOTAL_ROUND_COUNT / 4U)
                {
                    // NOLINTNEXTLINE(readability-magic-numbers)
                    return {(d & b) | ((~d) & c), static_cast<uint32_t>(((5U * CURRENT_ROUND) + 1U) % M.size())};
                }
                if (CURRENT_ROUND < 3U * TOTAL_ROUND_COUNT / 4U)
                {
                    // NOLINTNEXTLINE(readability-magic-numbers)
                    return {b ^ c ^ d, static_cast<uint32_t>(((3U * CURRENT_ROUND) + 5U) % M.size())};
                }
                // NOLINTNEXTLINE(readability-magic-numbers)
                return {c ^ (b | (~d)), static_cast<uint32_t>(static_cast<uint32_t>(7U * CURRENT_ROUND) % M.size())};
            }();

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            f = f + a + K[CURRENT_ROUND] + M[g];
            a = d;
            d = c;
            c = b;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            b = b + std::rotl(f, static_cast<int>(S[CURRENT_ROUND]));
        }
        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        digest[0] += a;
        digest[1] += b;
        digest[2] += c;
        digest[3] += d;
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    };

    // we process each 512 bit (64 byte) message chunk one by one
    // first we process all full 64 byte chunks from the input stream
    // then we make a separate view with the padded last chunk and process that separately

    auto              it                = std::ranges::begin(message);
    const auto        END               = std::ranges::end(message);
    const std::size_t FULL_CHUNKS_COUNT = TOTAL_BYTES / CHUNK_SIZE;

    for (const std::size_t _ : std::views::iota(std::size_t {0}, FULL_CHUNKS_COUNT))
    {
        processChunk(it);
        std::ranges::advance(it, CHUNK_SIZE);
    }

    // do the last chunk with the padding. Might be 2 chunks if the last bit of message is 56 bytes or more
    const auto MESSAGE_TAIL_VIEW = std::ranges::subrange(it, END);

    for (const auto& chunk :
         std::ranges::views::concat(MESSAGE_TAIL_VIEW, PADDING_VIEW) | std::ranges::views::chunk(CHUNK_SIZE))
    {
        processChunk(chunk.begin());
    }

    if constexpr (std::endian::native == std::endian::little)
    {
        return std::bit_cast<std::array<std::byte, MD5_DIGEST_LEN>>(digest);
    }
    else
    {
        std::array<std::byte, MD5_DIGEST_LEN> result {};
        for (const std::size_t IDX : std::ranges::views::iota(std::size_t {0}, MD5_DIGEST_LEN))
        {
            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,readability-magic-numbers)
            result[(IDX * 4U) + 0U] = static_cast<std::byte>(digest[IDX] >> 0U);
            result[(IDX * 4U) + 1U] = static_cast<std::byte>(digest[IDX] >> 8U);
            result[(IDX * 4U) + 2U] = static_cast<std::byte>(digest[IDX] >> 16U);
            result[(IDX * 4U) + 3U] = static_cast<std::byte>(digest[IDX] >> 24U);
            // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,readability-magic-numbers)
        }
        return result;
    }
}

constexpr auto parseHex(std::string_view hex) -> std::array<std::byte, MD5_DIGEST_LEN>
{
    std::array<std::byte, MD5_DIGEST_LEN> res {};
    for (const std::size_t IDX : std::ranges::views::iota(std::size_t {0}, MD5_DIGEST_LEN))
    {
        auto val = [](const char CHR)
        {
            static constexpr int BASE_10_FIRST_2_DIGIT_NUM {10};
            if (CHR >= 'a')
            {
                return static_cast<unsigned char>(CHR - 'a' + BASE_10_FIRST_2_DIGIT_NUM);
            }
            if (CHR >= 'A')
            {
                return static_cast<unsigned char>(CHR - 'A' + BASE_10_FIRST_2_DIGIT_NUM);
            }
            return static_cast<unsigned char>(CHR - '0');
        };

        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        res[IDX] = static_cast<std::byte>((val(hex[2U * IDX]) << 4U) + val(hex[(2 * IDX) + 1]));
    }
    return res;
}

[[nodiscard]]
constexpr auto digestToString(std::array<std::byte, MD5_DIGEST_LEN> digest) -> std::string
{
    std::array<char, MD5_DIGEST_LEN * 2> res {};
    for (const std::size_t IDX : std::ranges::views::iota(std::size_t {0}, MD5_DIGEST_LEN))
    {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access, readability-magic-numbers)
        res[2U * IDX]       = "0123456789abcdef"[static_cast<unsigned char>(digest[IDX] >> 4U) & 0x0fU];
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access, readability-magic-numbers)
        res[(2U * IDX) + 1] = "0123456789abcdef"[static_cast<unsigned char>(digest[IDX]) & 0x0fU];
    }
    return {res.begin(), res.end()};
}

[[nodiscard]]
constexpr auto stringToBytes(const std::string_view STR)
{
    return STR | std::views::transform([](const char CHR) { return static_cast<std::byte>(CHR); });
}

// make sure it's constexpr
// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
static_assert(stringToBytes("hello")[1] == static_cast<std::byte>('e'));

// some basic tests
static_assert(md5(stringToBytes("")) == parseHex("d41d8cd98f00b204e9800998ecf8427e"));
static_assert(md5(stringToBytes("hello")) == parseHex("5d41402abc4b2a76b9719d911017c592"));
// NOLINTBEGIN(readability-magic-numbers)
static_assert(md5(stringToBytes(std::string(999, 'A'))) == parseHex("81245975ac5c2db8d73f0704ba6c81c9"));

// check chunk boundary
static_assert(md5(stringToBytes(std::string(54, 'A'))) == parseHex("de05237f7d3965e0b33351893d23e05e"));
static_assert(md5(stringToBytes(std::string(55, 'A'))) == parseHex("e38a93ffe074a99b3fed47dfbe37db21"));
static_assert(md5(stringToBytes(std::string(56, 'A'))) == parseHex("a2f3e2024931bd470555002aa5ccc010"));
static_assert(md5(stringToBytes(std::string(57, 'A'))) == parseHex("9a7c38569e5a96e3cfbad45fb9ce5209"));
static_assert(md5(stringToBytes(std::string(58, 'A'))) == parseHex("ef843f60078dd0d52413dd05309f8503"));
static_assert(md5(stringToBytes(std::string(59, 'A'))) == parseHex("b0b5e976f4e7e61b01f13817aaf7da7e"));
static_assert(md5(stringToBytes(std::string(60, 'A'))) == parseHex("e009747e74dd24f3274fc71c240921b7"));
static_assert(md5(stringToBytes(std::string(61, 'A'))) == parseHex("14259830f67657a39cb0bdf5d6bb4e4b"));
static_assert(md5(stringToBytes(std::string(62, 'A'))) == parseHex("a5446e80abd7c822bf6a154887caea36"));
static_assert(md5(stringToBytes(std::string(63, 'A'))) == parseHex("5f1c4bb2970471a5c75b7ba1dc9ee3ed"));
static_assert(md5(stringToBytes(std::string(64, 'A'))) == parseHex("d289a97565bc2d27ac8b8545a5ddba45"));
static_assert(md5(stringToBytes(std::string(65, 'A'))) == parseHex("162b6d6eb17cd9da55f95f8c73a32dda"));
// NOLINTEND(readability-magic-numbers)

// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
static_assert(digestToString(md5(stringToBytes("")))[1] == '4');
