#include <Api/KttException.h>
#include <Repository/Utility.h>

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
} // namespace ktt::db
