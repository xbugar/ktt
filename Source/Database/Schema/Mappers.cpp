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

TuningSourceDto Mappers::MapSourceLoadRow(sqlite3_stmt* statement, const int columnOffset)
{
	TuningSourceDto source;
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

TuningRunLoadUdt Mappers::MapRunLoadRow(sqlite3_stmt* statement, const int columnOffset)
{
	TuningRunLoadUdt run;
	run.id = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 0));
	run.uuid = ReadTextColumn(statement, columnOffset + 1);
	run.createdAt = ReadTextColumn(statement, columnOffset + 2);
	return run;
}

std::optional<TuningResultLoadUdt> Mappers::MapResultLoadRow(sqlite3_stmt* statement, const int columnOffset,
	std::string* parseError)
{
	TuningResultLoadUdt result;
	result.id = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 0));
	result.duration = static_cast<std::size_t>(sqlite3_column_int64(statement, columnOffset + 1));

	const auto* resultJson = reinterpret_cast<const char*>(sqlite3_column_text(statement, columnOffset + 2));
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
