#include <optional>
#include <sqlite3.h>
#include <string>

namespace ktt::db
{
/** @class DatabaseUtility
 * Utility functions for SQLite database operations.
 * Provides helper methods for preparing SQL statements, binding parameters,
 * and extracting column values from query results.
 */
class DatabaseUtility
{
public:
    /** @fn static sqlite3_stmt *PrepareStatement(sqlite3 *connection, const char *sql, const char *errorPrefix)
     * Prepares a SQL statement for execution.
     * @param connection SQLite database connection.
     * @param sql The SQL query string to prepare.
     * @param errorPrefix Prefix message for error reporting.
     * @return Prepared SQLite statement pointer.
     * @throw KttException If statement preparation fails.
     */
    static sqlite3_stmt *PrepareStatement(sqlite3 *connection, const char *sql, const char *errorPrefix);

    /** @fn static size_t Sqlite3ColumnSizeT(sqlite3_stmt *statement, int columnIndex)
     * Extracts a size_t value from a query result column.
     * @param statement Prepared SQLite statement.
     * @param columnIndex Zero-based index of the column.
     * @return The column value as size_t.
     */
    static size_t Sqlite3ColumnSizeT(sqlite3_stmt *statement, int columnIndex);

    /** @fn static std::string Sqlite3ColumnString(sqlite3_stmt *statement, int columnIndex)
     * Extracts a string value from a query result column.
     * @param statement Prepared SQLite statement.
     * @param columnIndex Zero-based index of the column.
     * @return The column value as std::string, or empty string if NULL.
     */
    static std::string Sqlite3ColumnString(sqlite3_stmt *statement, int columnIndex);

    /** @fn static std::string ReadTextColumn(sqlite3_stmt *statement, const int column)
     * Reads a TEXT column value from a query result.
     * @param statement Prepared SQLite statement.
     * @param column Zero-based index of the column.
     * @return The column value as std::string, or empty string if NULL.
     */
    static std::string ReadTextColumn(sqlite3_stmt *statement, const int column);

    /** @fn static void BindOptionalInt(sqlite3_stmt *statement, int index, const std::optional<int> &value)
     * Binds an optional integer value to a prepared statement parameter.
     * Binds NULL if the optional is empty.
     * @param statement Prepared SQLite statement.
     * @param index One-based parameter index.
     * @param value Optional integer value to bind.
     */
    static void BindOptionalInt(sqlite3_stmt *statement, int index, const std::optional<int> &value);

    /** @fn static void BindOptionalText(sqlite3_stmt *statement, int index, const std::optional<std::string> &value)
     * Binds an optional string value to a prepared statement parameter.
     * Binds NULL if the optional is empty.
     * @param statement Prepared SQLite statement.
     * @param index One-based parameter index.
     * @param value Optional string value to bind.
     */
    static void BindOptionalText(sqlite3_stmt *statement, int index, const std::optional<std::string> &value);


    static std::string SqlList(const uint size);
};

} // namespace ktt::db
