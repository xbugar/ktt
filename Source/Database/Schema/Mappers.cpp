#include <sqlite3.h>

#include <Database/Schema/Mappers.h>

namespace ktt
{

namespace
{

std::string ReadTextColumn(sqlite3_stmt* statement, const int column)
{
	const auto* text = reinterpret_cast<const char*>(sqlite3_column_text(statement, column));
	return text != nullptr ? text : "";
}

} // namespace

TuningSourceLoadUdt Mappers::MapSourceLoadRow(sqlite3_stmt* statement, const int columnOffset)
{
	TuningSourceLoadUdt source;
	source.id = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 0));
	source.parameterFingerprint = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 1));
	source.sourceFingerprint = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 2));
	source.createdAt = ReadTextColumn(statement, columnOffset + 3);
	return source;
}

TuningSpaceLoadUdt Mappers::MapSpaceLoadRow(sqlite3_stmt* statement, const int columnOffset)
{
	TuningSpaceLoadUdt space;
	space.id = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 0));
	space.sourceId = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 1));
	space.spaceFingerprint = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 2));
	space.createdAt = ReadTextColumn(statement, columnOffset + 3);
	return space;
}

std::optional<TuningResultLoadUdt> Mappers::MapResultLoadRow(sqlite3_stmt* statement, const int columnOffset,
	std::string* parseError)
{
	TuningResultLoadUdt result;
	result.id = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 0));
	result.runId = ReadTextColumn(statement, columnOffset + 1);
	result.spaceId = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 2));
	result.duration = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 3));

	const auto* resultJson = reinterpret_cast<const char*>(sqlite3_column_text(statement, columnOffset + 4));
	if (resultJson != nullptr)
	{
		try
		{
			result.result = json::parse(resultJson);
		}
		catch (const json::parse_error& e)
		{
			if (parseError != nullptr)
			{
				*parseError = e.what();
			}
			return std::nullopt;
		}
	}

	return result;
}

} // namespace ktt
