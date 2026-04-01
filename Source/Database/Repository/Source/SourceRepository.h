#pragma once
#include <memory>
#include <sqlite3.h>

#include <Database/Schema/Udts/TuningSourceUdt.h>


namespace ktt
{

class SourceRepository
{
public:
    static size_t CreateSource(
        sqlite3* connection,
        const TuningSourceSaveUdt& source);

    static std::unique_ptr<TuningSourceDto> SelectSourceByFingerprints(
        sqlite3 *connection,
        size_t sourceFingerprint,
        size_t parameterFingerprint);
};

} // namespace ktt
