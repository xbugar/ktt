#pragma once

#include <sqlite3.h>
namespace ktt
{

class Schema
{
public:
    Schema() = delete;
    static void CreateIfNotExists(sqlite3 *connection);
};

} // namespace ktt
