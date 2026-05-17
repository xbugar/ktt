#include <optional>
#include <sqlite3.h>
#include <string>

namespace ktt::db
{
class DatabaseUtility
{
public:
    static sqlite3_stmt *PrepareStatement(sqlite3 *connection, const char *sql, const char *errorPrefix);
    static size_t Sqlite3ColumnSizeT(sqlite3_stmt *statement, int columnIndex);
    static std::string Sqlite3ColumnString(sqlite3_stmt *statement, int columnIndex);
    static std::string ReadTextColumn(sqlite3_stmt *statement, const int column);
    static void BindOptionalInt(sqlite3_stmt *statement, int index, const std::optional<int> &value);
    static void BindOptionalText(sqlite3_stmt *statement, int index, const std::optional<std::string> &value);
};

} // namespace ktt::db
