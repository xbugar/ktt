#pragma once

#include <vector>

#include <Database/Schema/Udts/TuningSourceUdt.h>
#include <Database/Schema/Udts/TuningSpaceUdt.h>

namespace ktt
{

struct SaveTuningUdt
{
    TuningSourceSaveUdt source;
    TuningSpaceSaveUdt space;
    std::vector<TuningResultSaveUdt> result;
};

struct LoadTuningUdt
{
    std::vector<TuningSourceLoadUdt> sources;
};

} // namespace ktt
