#include "md5.hpp"
#include <cstddef>
#include <ranges>
#include <string_view>


namespace {
[[nodiscard]]
constexpr auto stringToBytes(const std::string_view STR)
{
    return STR | std::ranges::views::transform([](const char CHR) { return static_cast<std::byte>(CHR); });
}
}

// make sure it's constexpr
// NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
static_assert(stringToBytes("hello")[1] == static_cast<std::byte>('e'));

// some basic tests
static_assert(MD5::md5(stringToBytes("")) == MD5::Digest("d41d8cd98f00b204e9800998ecf8427e"));
static_assert(MD5::md5(stringToBytes("hello")) == MD5::Digest("5d41402abc4b2a76b9719d911017c592"));
// NOLINTBEGIN(readability-magic-numbers)
static_assert(MD5::md5(stringToBytes(std::string(999, 'A'))) == MD5::Digest("81245975ac5c2db8d73f0704ba6c81c9"));

// check chunk boundary
static_assert(MD5::md5(stringToBytes(std::string(54, 'A'))) == MD5::Digest("de05237f7d3965e0b33351893d23e05e"));
static_assert(MD5::md5(stringToBytes(std::string(55, 'A'))) == MD5::Digest("e38a93ffe074a99b3fed47dfbe37db21"));
static_assert(MD5::md5(stringToBytes(std::string(56, 'A'))) == MD5::Digest("a2f3e2024931bd470555002aa5ccc010"));
static_assert(MD5::md5(stringToBytes(std::string(57, 'A'))) == MD5::Digest("9a7c38569e5a96e3cfbad45fb9ce5209"));
static_assert(MD5::md5(stringToBytes(std::string(58, 'A'))) == MD5::Digest("ef843f60078dd0d52413dd05309f8503"));
static_assert(MD5::md5(stringToBytes(std::string(59, 'A'))) == MD5::Digest("b0b5e976f4e7e61b01f13817aaf7da7e"));
static_assert(MD5::md5(stringToBytes(std::string(60, 'A'))) == MD5::Digest("e009747e74dd24f3274fc71c240921b7"));
static_assert(MD5::md5(stringToBytes(std::string(61, 'A'))) == MD5::Digest("14259830f67657a39cb0bdf5d6bb4e4b"));
static_assert(MD5::md5(stringToBytes(std::string(62, 'A'))) == MD5::Digest("a5446e80abd7c822bf6a154887caea36"));
static_assert(MD5::md5(stringToBytes(std::string(63, 'A'))) == MD5::Digest("5f1c4bb2970471a5c75b7ba1dc9ee3ed"));
static_assert(MD5::md5(stringToBytes(std::string(64, 'A'))) == MD5::Digest("d289a97565bc2d27ac8b8545a5ddba45"));
static_assert(MD5::md5(stringToBytes(std::string(65, 'A'))) == MD5::Digest("162b6d6eb17cd9da55f95f8c73a32dda"));
// NOLINTEND(readability-magic-numbers)

static_assert(MD5::md5(stringToBytes("")).toString() == "d41d8cd98f00b204e9800998ecf8427e");
