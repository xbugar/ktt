#include <chrono>
#include <cstdint>
#include <mutex>
#include <random>

#include <Api/KttException.h>
#include <Database/Utility/Uuid.h>

namespace ktt::db
{

uuid UuidGenerator::GenerateUuid()
{
    std::random_device rd;
    uuid id;

    for (auto& b : id)
        b = static_cast<std::uint8_t>(rd());

    // set version (v4)
    id[6] = (id[6] & 0x0F) | 0x40;

    // set variant
    id[8] = (id[8] & 0x3F) | 0x80;

    return id;
}

uuid UuidGenerator::GenerateUuidV7()
{
    // One generator per process, seeded once from the OS; the mutex makes concurrent calls safe.
    static std::mutex mutex;
    static std::mt19937_64 engine{[] {
        std::random_device rd;
        std::seed_seq seed{rd(), rd(), rd(), rd(), rd(), rd(), rd(), rd()};
        return std::mt19937_64{seed};
    }()};

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto ms = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());

    std::uint64_t random[2];
    {
        std::lock_guard<std::mutex> lock(mutex);
        random[0] = engine();
        random[1] = engine();
    }

    uuid id;

    // bytes 0-5: 48-bit Unix timestamp in milliseconds, big-endian
    for (int i = 0; i < 6; ++i)
        id[i] = static_cast<std::uint8_t>(ms >> (8 * (5 - i)));

    // bytes 6-15: random
    for (int i = 0; i < 8; ++i)
        id[6 + i] = static_cast<std::uint8_t>(random[0] >> (8 * i));
    id[14] = static_cast<std::uint8_t>(random[1]);
    id[15] = static_cast<std::uint8_t>(random[1] >> 8);

    // set version (v7)
    id[6] = (id[6] & 0x0F) | 0x70;

    // set variant
    id[8] = (id[8] & 0x3F) | 0x80;

    return id;
}

std::uint8_t UuidGenerator::GetVersion(const uuid& id)
{
    return static_cast<std::uint8_t>(id[6] >> 4);
}

std::string UuidGenerator::ToString(const uuid& id)
{
    static constexpr char HexDigits[] = "0123456789abcdef";
    std::string text;
    text.reserve(36);

    for (size_t i = 0; i < id.size(); ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            text += '-';

        text += HexDigits[id[i] >> 4];
        text += HexDigits[id[i] & 0x0F];
    }

    return text;
}

uuid UuidGenerator::Parse(const std::string& text)
{
    const auto hexValue = [](const char c) -> int
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        return -1;
    };

    const auto invalid = [&text]()
    {
        return KttException("Invalid UUID: \"" + text + "\"");
    };

    if (text.size() != 36)
        throw invalid();

    uuid id{};
    size_t position = 0;

    for (size_t i = 0; i < id.size(); ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
        {
            if (text[position] != '-')
                throw invalid();

            ++position;
        }

        const int high = hexValue(text[position]);
        const int low = hexValue(text[position + 1]);

        if (high < 0 || low < 0)
            throw invalid();

        id[i] = static_cast<std::uint8_t>((high << 4) | low);
        position += 2;
    }

    return id;
}

} // namespace ktt::db
