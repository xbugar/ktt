#include <sqlite3.h>

#include <Database/Schema/Udts/TuningSpaceUdt.h>
#include <Database/Repository/Utility.h>
#include "TuningSpaceUdt.h"

namespace ktt
{

TuningSpaceLoadUdt TuningSpaceLoadUdt::FromRow(sqlite3_stmt* statement)
{
	TuningSpaceLoadUdt space;
	space.id = static_cast<std::size_t>(sqlite3_column_int64(statement, 0));
	space.sourceId = static_cast<std::size_t>(sqlite3_column_int64(statement, 1));
	space.spaceFingerprint = static_cast<std::size_t>(sqlite3_column_int64(statement, 2));
	space.createdAt = DatabaseUtility::ReadTextColumn(statement, 3);
	return space;
}

} // namespace ktt
