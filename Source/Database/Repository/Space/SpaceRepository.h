#pragma once

#include <memory>
#include <sqlite3.h>

#include <Database/Schema/Udts/TuningSpaceUdt.h>

namespace ktt
{

class SpaceRepository
{
public:
    static std::unique_ptr<TuningSpaceLoadUdt> SelectSpaceByFingerprint(
        sqlite3* connection,
        size_t sourceId,
        size_t spaceFingerprint);

    static size_t CreateSpace(
        sqlite3* connection,
        const TuningSpaceSaveUdt& space);
};

} // namespace ktt

