
#include <Api/KttException.h>
#include <Database/Repository/Utility.h>

namespace ktt
{
sqlite3_stmt *DatabaseUtility::PrepareStatement(sqlite3 *connection, const char *sql, const char *errorPrefix)
{
    sqlite3_stmt *statement = nullptr;
    const int result = sqlite3_prepare_v2(connection, sql, -1, &statement, nullptr);

    if (result != SQLITE_OK)
    {
        throw KttException(std::string(errorPrefix) + sqlite3_errmsg(connection), ExceptionReason::Database);
    }

    return statement;
}

std::string DatabaseUtility::Sqlite3ColumnString(sqlite3_stmt *statement, int columnIndex)
{
    const auto *text = reinterpret_cast<const char *>(sqlite3_column_text(statement, columnIndex));
    return text != nullptr ? text : "";
}

std::string DatabaseUtility::ReadTextColumn(sqlite3_stmt* statement, const int column)
{
	const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
	return text != nullptr ? text : "";
}
} // namespace ktt
