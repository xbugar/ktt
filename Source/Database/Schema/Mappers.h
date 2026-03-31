#pragma once

#include <optional>
#include <string>

#include <Database/Schema/Udts/TuningResultUdt.h>
#include <Database/Schema/Udts/TuningSourceUdt.h>
#include <Database/Schema/Udts/TuningSpaceUdt.h>

struct sqlite3_stmt;

namespace ktt
{

class Mappers
{
public:
	static TuningSourceLoadUdt MapSourceLoadRow(sqlite3_stmt* statement, int columnOffset = 0);
	static TuningSpaceLoadUdt MapSpaceLoadRow(sqlite3_stmt* statement, int columnOffset = 0);
	static std::optional<TuningResultLoadUdt> MapResultLoadRow(sqlite3_stmt* statement, int columnOffset = 0,
		std::string* parseError = nullptr);
};

} // namespace ktt

