#include <Api/KttException.h>
#include <Database/Utility/TimeUnitGuard.h>
#include <Output/TimeConfiguration/TimeConfiguration.h>

namespace ktt::db
{

std::string TimeUnitToName(const TimeUnit unit)
{
    switch (unit)
    {
    case TimeUnit::Nanoseconds:
        return "nanoseconds";
    case TimeUnit::Microseconds:
        return "microseconds";
    case TimeUnit::Milliseconds:
        return "milliseconds";
    case TimeUnit::Seconds:
        return "seconds";
    }

    throw KttException("Unknown time unit value " + std::to_string(static_cast<int>(unit)));
}

TimeUnit TimeUnitFromName(const std::string& name)
{
    for (const auto unit : {TimeUnit::Nanoseconds, TimeUnit::Microseconds, TimeUnit::Milliseconds, TimeUnit::Seconds})
    {
        if (TimeUnitToName(unit) == name)
            return unit;
    }

    throw KttException("Unknown time unit name '" + name + "'");
}

std::unique_lock<std::recursive_mutex> LockTimeUnit()
{
    static std::recursive_mutex mutex;
    return std::unique_lock<std::recursive_mutex>(mutex);
}

TimeUnitGuard::TimeUnitGuard(const TimeUnit unit) :
    m_Lock(LockTimeUnit()),
    m_PreviousUnit(TimeConfiguration::GetInstance().GetTimeUnit())
{
    TimeConfiguration::GetInstance().SetTimeUnit(unit);
}

TimeUnitGuard::~TimeUnitGuard()
{
    TimeConfiguration::GetInstance().SetTimeUnit(m_PreviousUnit);
}

} // namespace ktt::db
