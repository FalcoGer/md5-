#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gcem.hpp>
#include <iterator>
#include <memory>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

// TODO: Test in big endian architectures
// FIXME: big endian in Chunk creation, just look at all the bit_casts, which should all be broken for big endian
// TOOD: split Digest into Digest (value type) and Hasher with an update and finialize version.
// TODO: fix performance
// TODO: runtime tests (catch2)
// TODO: update all test results in readme
// TOOD: write a bash script to do the performance test for me.
// TODO: catch2 benchmark?
// TODO: allow temporary iterators for Chunk constructor

namespace MD5
{

static_assert(
  std::endian::native == std::endian::little || std::endian::native == std::endian::big,
  "Native endianness not supported"
);

class Digest;
class Chunk
{
    static constexpr std::size_t CHUNK_SIZE_BYTES = 64;
    static constexpr std::size_t CHUNK_SIZE_WORDS = CHUNK_SIZE_BYTES / sizeof(std::uint32_t);

  public:
    constexpr Chunk() = delete;
    constexpr explicit Chunk(const std::array<std::byte, CHUNK_SIZE_BYTES>& chunk)
            : m_data {std::bit_cast<decltype(m_data)>(chunk)}
    {}
    constexpr explicit Chunk(std::forward_iterator auto& chunkIt)
        requires std::same_as<std::iter_value_t<decltype(chunkIt)>, std::byte>
    {
        if consteval
        {
            for (std::uint32_t& word : m_data)
            {
                // NOLINTBEGIN(readability-magic-numbers)
                word  = static_cast<std::uint32_t>(*chunkIt++) << 0U;
                word |= static_cast<std::uint32_t>(*chunkIt++) << 8U;
                word |= static_cast<std::uint32_t>(*chunkIt++) << 16U;
                word |= static_cast<std::uint32_t>(*chunkIt++) << 24U;
                // NOLINTEND(readability-magic-numbers)
            }
        }
        else
        {
            if constexpr (std::endian::native == std::endian::little)
            {
                if constexpr (std::contiguous_iterator<decltype(chunkIt)>)
                {
                    std::memcpy(m_data.data(), std::to_address(chunkIt), CHUNK_SIZE_BYTES);
                    std::advance(chunkIt, sizeof(std::uint32_t));
                }
                else
                {
                    auto bytes = std::as_writable_bytes(std::span {m_data});
                    chunkIt = std::ranges::copy_n(chunkIt, CHUNK_SIZE_BYTES, bytes.begin()).in;
                }
            }
            else
            {
                for (std::uint32_t& word : m_data)
                {
                    // NOLINTBEGIN(readability-magic-numbers)
                    word  = static_cast<std::uint32_t>(*chunkIt++) << 0U;
                    word |= static_cast<std::uint32_t>(*chunkIt++) << 8U;
                    word |= static_cast<std::uint32_t>(*chunkIt++) << 16U;
                    word |= static_cast<std::uint32_t>(*chunkIt++) << 24U;
                    // NOLINTEND(readability-magic-numbers)
                }
            }
        }
    }

    constexpr Chunk(const Chunk&)                     = default;
    constexpr Chunk(Chunk&&)                          = default;
    constexpr auto operator= (const Chunk&) -> Chunk& = default;
    constexpr auto operator= (Chunk&&) -> Chunk&      = default;
    constexpr ~Chunk()                                = default;
    static constexpr std::size_t size() { return CHUNK_SIZE_BYTES; }

  private:
    // 16 * 32 bit words
    std::array<std::uint32_t, CHUNK_SIZE_WORDS> m_data {};
    friend Digest;
};

class Digest
{
    static constexpr std::size_t MD5_DIGEST_LEN = 16;

  public:
    constexpr Digest()  = default;
    constexpr ~Digest() = default;
    constexpr explicit Digest(const std::array<std::byte, MD5_DIGEST_LEN>& digest)
            : m_data {std::bit_cast<decltype(m_data)>(digest)}
    {
        // empty
    }
    constexpr explicit Digest(const std::array<std::uint32_t, MD5_DIGEST_LEN / sizeof(std::uint32_t)>& digest)
            : m_data {digest}
    {
        // empty
    }
    constexpr Digest(const Digest&)                     = default;
    constexpr Digest(Digest&&)                          = default;
    constexpr auto operator= (const Digest&) -> Digest& = default;
    constexpr auto operator= (Digest&&) -> Digest&      = default;
    constexpr auto operator== (const Digest& other) const -> bool { return m_data == other.m_data; }
    constexpr auto operator!= (const Digest& other) const -> bool { return m_data != other.m_data; }

    constexpr Digest(
      const std::string_view HEX
    )    // NOLINT(google-explicit-constructor,cppcoreguidelines-explicit-constructor,misc-explicit-constructor)
    {
        if (HEX.size() != MD5_DIGEST_LEN * 2)
        {
            throw std::invalid_argument {"Invalid hex length"};
        }

        std::array<std::byte, MD5_DIGEST_LEN> result {};
        for (const std::size_t IDX : std::ranges::views::iota(std::size_t {0}, MD5_DIGEST_LEN))
        {
            static constexpr auto VAL = [](const char CHR)
            {
                static constexpr int BASE_10_FIRST_2_DIGIT_NUM {10};
                if (CHR >= 'a' && CHR <= 'f')
                {
                    return static_cast<unsigned char>(CHR - 'a' + BASE_10_FIRST_2_DIGIT_NUM);
                }
                if (CHR >= 'A' && CHR <= 'F')
                {
                    return static_cast<unsigned char>(CHR - 'A' + BASE_10_FIRST_2_DIGIT_NUM);
                }
                if (CHR >= '0' && CHR <= '9')
                {
                    return static_cast<unsigned char>(CHR - '0');
                }
                throw std::invalid_argument {"Invalid hex character"};
            };

            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            result[IDX] = static_cast<std::byte>((VAL(HEX[2U * IDX]) << 4U) + VAL(HEX[(2 * IDX) + 1]));
        }
        m_data = std::bit_cast<decltype(m_data)>(result);
    }

    static constexpr std::size_t size() { return MD5_DIGEST_LEN; }
    [[nodiscard]]
    constexpr auto toString() const -> std::string
    {
        auto                                 bytes = toBytes();
        std::array<char, MD5_DIGEST_LEN * 2> res {};
        for (const std::size_t IDX : std::ranges::views::iota(std::size_t {0}, MD5_DIGEST_LEN))
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access, readability-magic-numbers)
            res[2U * IDX]       = "0123456789abcdef"[static_cast<unsigned char>(bytes[IDX] >> 4U) & 0x0fU];
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access, readability-magic-numbers)
            res[(2U * IDX) + 1] = "0123456789abcdef"[static_cast<unsigned char>(bytes[IDX]) & 0x0fU];
        }
        return {res.begin(), res.end()};
    }

    [[nodiscard]]
    constexpr auto toBytes() const -> std::array<std::byte, MD5_DIGEST_LEN>
    {
        return std::bit_cast<std::array<std::byte, MD5_DIGEST_LEN>>(m_data);
    }

    constexpr void processChunk(const Chunk& chunk)
    {
        // Init hash values for this chunk
        auto [a, b, c, d]       = m_data;    // NOLINT(readability-identifier-length)

        // Run the Md5 hashing algorithm main loop (64 times)
        // Every 16 times the calculation changes for the f and g values.

        auto applyRoundToDigest = [&a, &b, &c, &d, &chunk](
                                    std::uint32_t& f, const std::uint32_t G, const std::uint32_t CURRENT_ROUND
                                  )    // NOLINT(readability-identifier-length)
        {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            f = f + a + K[CURRENT_ROUND] + chunk.m_data[G];
            a = d;
            d = c;
            c = b;
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
            b = b + std::rotl(f, static_cast<int>(S[CURRENT_ROUND]));
        };

        // first round for initialization of f and g outside the loop.
        std::uint32_t f {};
        std::uint32_t g {};

        for (const std::uint32_t CURRENT_ROUND :
             std::ranges::views::iota(std::uint32_t {0}, static_cast<std::uint32_t>(TOTAL_ROUND_COUNT / 4U)))
        {
            // 0 .. 15
            f = (b & c) | ((~b) & d);
            g = CURRENT_ROUND;
            applyRoundToDigest(f, g, CURRENT_ROUND);
        }
        for (const std::uint32_t CURRENT_ROUND : std::ranges::views::iota(
               static_cast<std::uint32_t>(TOTAL_ROUND_COUNT / 4U),
               static_cast<std::uint32_t>(2U * TOTAL_ROUND_COUNT / 4U)
             ))
        {
            // 16 .. 31
            f = (d & b) | ((~d) & c);
            // NOLINTNEXTLINE(readability-magic-numbers)
            g = static_cast<std::uint32_t>(((5U * CURRENT_ROUND) + 1U) % Chunk::CHUNK_SIZE_WORDS);
            applyRoundToDigest(f, g, CURRENT_ROUND);
        }
        for (const std::uint32_t CURRENT_ROUND : std::ranges::views::iota(
               static_cast<std::uint32_t>(2U * TOTAL_ROUND_COUNT / 4U),
               static_cast<std::uint32_t>(3U * TOTAL_ROUND_COUNT / 4U)
             ))
        {
            // 32 .. 47
            f = b ^ c ^ d;
            // NOLINTNEXTLINE(readability-magic-numbers)
            g = static_cast<std::uint32_t>(((3U * CURRENT_ROUND) + 5U) % Chunk::CHUNK_SIZE_WORDS);
            applyRoundToDigest(f, g, CURRENT_ROUND);
        }
        for (const std::uint32_t CURRENT_ROUND : std::ranges::views::iota(
               static_cast<std::uint32_t>(3U * TOTAL_ROUND_COUNT / 4U), static_cast<std::uint32_t>(TOTAL_ROUND_COUNT)
             ))
        {
            // 48 .. 63
            f = c ^ (b | (~d));
            // NOLINTNEXTLINE(readability-magic-numbers)
            g = static_cast<std::uint32_t>((static_cast<std::size_t>(7U * CURRENT_ROUND)) % Chunk::CHUNK_SIZE_WORDS);
            applyRoundToDigest(f, g, CURRENT_ROUND);
        }

        // NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
        m_data[0] += a;
        m_data[1] += b;
        m_data[2] += c;
        m_data[3] += d;
        // NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    }

    constexpr void digestTail(
      const std::ranges::forward_range auto& messageTailSubrange, const std::size_t TOTAL_MESSAGE_SIZE_IN_BYTES
    )
        requires std::same_as<std::ranges::range_value_t<decltype(messageTailSubrange)>, std::byte>
    {
        // 1. Unconditionally append a single 1 bit (for whole byte messages we append 0x80)
        // 2. Then append zeroes until message bit length is 448 bits mod 512 (or 56 bytes mod 64)
        // 3. Then append original message length in bits mod 2^64
        // This creates either 1 or 2 chunks of exactly 64 bytes

        const std::uint64_t TOTAL_BITS             = std::uint64_t {TOTAL_MESSAGE_SIZE_IN_BYTES}
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
        const auto ZEROES_COUNT = (Chunk::size()
                                   - sizeof(TOTAL_BITS)
                                   - ((TOTAL_MESSAGE_SIZE_IN_BYTES + 1) % Chunk::size())
                                   + Chunk::size())
                                  % Chunk::size();

        const auto PADDING_VIEW = std::views::concat(
          std::views::single(std::byte {0x80}),
          std::views::repeat(std::byte {0x00}, ZEROES_COUNT),
          MESSAGE_LEN_INFO_BYTES
        );

        // We can't just return a view pointing into local variables. So we digest right here.
        for (const auto& chunkView :
             std::ranges::views::concat(messageTailSubrange, PADDING_VIEW) | std::ranges::views::chunk(Chunk::size()))
        {
            auto it = std::begin(chunkView);
            processChunk(Chunk(it));
        }
    }

  private:
    // NOLINTBEGIN(readability-magic-numbers)
    // clang-format off
    std::array<std::uint32_t, MD5_DIGEST_LEN / sizeof(std::uint32_t)> m_data
    {{
      0x67'45'23'01,    // A
      0xef'cd'ab'89,    // B
      0x98'ba'dc'fe,    // C
      0x10'32'54'76,    // D
    }}; // NOLINT(readability-trailing-comma)
    // clang-format on
    // NOLINTEND(readability-magic-numbers)

    static constexpr std::size_t                                  TOTAL_ROUND_COUNT = 64;
    // shift amounts for every idx-th round
    // clang-format off
    static constexpr std::array<std::uint32_t, TOTAL_ROUND_COUNT> S                 =
    {{
     7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
     14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
     4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21,
    }}; // NOLINT(readability-trailing-comma)
    // clang-format on

    static constexpr std::array<std::uint32_t, TOTAL_ROUND_COUNT> K                 = []
    {
        std::array<std::uint32_t, TOTAL_ROUND_COUNT> result {};
        for (auto&& [idx, val] : result | std::ranges::views::enumerate)
        {
            //NOLINTNEXTLINE(readability-magic-numbers)
            val = static_cast<std::uint32_t>(gcem::floor(gcem::pow(2.0, 32.0) * gcem::abs(gcem::sin(idx + 1))));
        }
        return result;
    }();
};


[[nodiscard]]
constexpr auto md5(const std::ranges::forward_range auto& message) -> Digest
    requires std::same_as<std::ranges::range_value_t<decltype(message)>, std::byte>
             && std::ranges::sized_range<decltype(message)>
{
    auto              digest            = Digest {};

    const std::size_t TOTAL_BYTES       = std::ranges::size(message);


    // we process each 512 bit (64 byte) message chunk one by one
    // first we process all full 64 byte chunks from the input stream
    // then we make a separate view with the padded last chunk and process that separately

    auto              it                = std::ranges::begin(message);
    const auto        END               = std::ranges::end(message);
    const std::size_t FULL_CHUNKS_COUNT = TOTAL_BYTES / Chunk::size();

    for (const std::size_t _ : std::views::iota(std::size_t {0}, FULL_CHUNKS_COUNT))
    {
        digest.processChunk(Chunk(it));
        // std::ranges::advance(it, Chunk::size());
    }

    digest.digestTail(std::ranges::subrange(it, END), TOTAL_BYTES);

    return digest;
}
}    // namespace MD5
