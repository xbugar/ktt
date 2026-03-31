#pragma once

#include <cstddef>
#include <memory>

struct sqlite3;

namespace ktt
{

struct Record;
struct TuningSourceSaveUdt;
struct TuningSourceLoadUdt;

class StatementRepository
{
public:
    static void UpdateTuningRecord(sqlite3* connection, const Record& record);
    static void InsertTuningRecord(sqlite3* connection, const Record& record);
    static std::unique_ptr<Record> SelectTuningRecord(sqlite3* connection, const Record& data);
    static std::unique_ptr<TuningSourceLoadUdt> SelectSourceByFingerprints(sqlite3* connection,
        const TuningSourceSaveUdt& source);
    static void SelectTopResultsForSourceId(sqlite3* connection, std::size_t sourceId, TuningSourceLoadUdt& source,
        std::size_t limit = 5);
};

} // namespace ktt



