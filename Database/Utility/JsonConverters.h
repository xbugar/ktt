#pragma once

#include <json.hpp>

namespace ktt::db
{
struct SourceStats;

void to_json(nlohmann::json &j, const SourceStats &stats);
void from_json(const nlohmann::json &j, SourceStats &stats);

} // namespace ktt::db
