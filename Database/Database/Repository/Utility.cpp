#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <random>
#include <sstream>

#include <pugixml.hpp>

#include <Api/KttException.h>
#include <Database/Repository/Utility.h>
#include <Database/Utility/ResultSerializationJsonT4.h>
#include <Output/JsonConverters.h>
#include <Output/XmlConverters.h>

namespace ktt::db
{
sqlite3_stmt *DatabaseUtility::PrepareStatement(sqlite3 *connection, const char *sql, const char *errorPrefix)
{
    sqlite3_stmt *statement = nullptr;
    const int result = sqlite3_prepare_v2(connection, sql, -1, &statement, nullptr);

    if (result != SQLITE_OK)
    {
        throw KttException(std::string(errorPrefix) + sqlite3_errmsg(connection));
    }

    return statement;
}

std::string DatabaseUtility::Sqlite3ColumnString(sqlite3_stmt *statement, int columnIndex)
{
    const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(statement, columnIndex));
    return text != nullptr ? text : "";
}

std::string DatabaseUtility::ReadTextColumn(sqlite3_stmt *statement, const int column)
{
    const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(statement, column));
    return text != nullptr ? text : "";
}

void DatabaseUtility::BindOptionalInt(sqlite3_stmt *statement, const int index, const std::optional<int> &value)
{
    if (value)
        sqlite3_bind_int(statement, index, *value);
    else
        sqlite3_bind_null(statement, index);
}

void DatabaseUtility::BindOptionalText(
    sqlite3_stmt *statement, const int index, const std::optional<std::string> &value
)
{
    if (value)
        sqlite3_bind_text(statement, index, value->c_str(), -1, SQLITE_TRANSIENT);
    else
        sqlite3_bind_null(statement, index);
}

void DatabaseUtility::BindUuid(sqlite3_stmt *statement, const int index, const uuid &value)
{
    sqlite3_bind_blob(statement, index, value.data(), value.size(), SQLITE_TRANSIENT);
}

uuid DatabaseUtility::ReadUuidColumn(sqlite3_stmt *statement, const int column)
{
    const auto *bytes = static_cast<const std::uint8_t *>(sqlite3_column_blob(statement, column));
    const int size = sqlite3_column_bytes(statement, column);

    if (bytes == nullptr || size != 16)
        throw KttException("Failed to read UUID column: expected a 16-byte BLOB");

    uuid id;
    std::copy(bytes, bytes + 16, id.begin());
    return id;
}

std::string DatabaseUtility::SqlList(const uint size)
{
    std::string sqlList = "(";
    for (size_t i = 0; i < size; ++i)
    {
        if (i > 0)
            sqlList += ", ";
        sqlList += "?";
    }
    sqlList += ")";
    return sqlList;
}

std::string DatabaseUtility::SerializeResult(const KernelResult &result, const ktt::OutputFormat format, const int indent)
{
    switch (format)
    {
    case ktt::OutputFormat::JSON_T4:
        return SerializeResultJsonT4(result, indent);
    case ktt::OutputFormat::XML:
    {
        pugi::xml_document document;
        AppendKernelResult(document, result);
        std::ostringstream stream;
        document.save(stream);
        return stream.str();
    }
    case ktt::OutputFormat::JSON:
    default:
        return nlohmann::json(result).dump(indent);
    }
}

KernelResult DatabaseUtility::DeserializeResult(const std::string &text, const ktt::OutputFormat format)
{
    switch (format)
    {
    case ktt::OutputFormat::JSON_T4:
        return DeserializeResultJsonT4(text);
    case ktt::OutputFormat::XML:
    {
        pugi::xml_document document;
        document.load_string(text.c_str());
        return ParseKernelResult(document.child("KernelResult"));
    }
    case ktt::OutputFormat::JSON:
    default:
        return nlohmann::json::parse(text).get<KernelResult>();
    }
}
} // namespace ktt::db
