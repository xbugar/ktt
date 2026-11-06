#pragma once

#include <string>

#include <Output/TimeConfiguration/TimeUnit.h>

namespace ktt::db
{

/** @fn std::string TimeUnitToName(TimeUnit unit)
 * Returns the name a time unit is stored under: "nanoseconds", "microseconds", "milliseconds" or "seconds".
 * These are the tuning_run.time_unit values, and the same lowercase names the ktt-db server uses. The JSON_T4
 * converter in Source/Output has a similar table, but it follows the T4 format; this one follows the stored values and
 * the server API, so it is kept separate on purpose.
 * @param unit Time unit to name.
 * @return The stored name of the unit.
 * @throw KttException If the unit has no name.
 */
std::string TimeUnitToName(TimeUnit unit);

/** @fn TimeUnit TimeUnitFromName(const std::string& name)
 * Parses a time unit name written by TimeUnitToName.
 * @param name Stored name of the unit.
 * @return The time unit.
 * @throw KttException If the name is not one of the four unit names.
 */
TimeUnit TimeUnitFromName(const std::string& name);

/** @class TimeUnitGuard
 * RAII helper that sets KTT's global time unit (TimeConfiguration) for the lifetime of the guard and restores the
 * previous unit on destruction, also when an exception propagates.
 *
 * Serialized kernel results carry their durations in the time unit that was active when they were written, and the
 * KTT converters read them back with the unit that is active now. The guard makes the converters use the unit a run
 * was saved with, without changing the converters in Source/.
 *
 * Not thread-safe: the time unit is process-wide, so other threads that read or set it while a guard is alive see
 * the guard's unit, and a concurrent Tuner::SetTimeUnit can be undone when the guard restores the old unit.
 */
class TimeUnitGuard
{
public:
    /** @fn explicit TimeUnitGuard(TimeUnit unit)
     * Remembers the current time unit and switches to the given one.
     * @param unit Time unit to use while the guard is alive.
     */
    explicit TimeUnitGuard(TimeUnit unit);

    /** Restores the time unit that was active when the guard was constructed. */
    ~TimeUnitGuard();

    TimeUnitGuard(const TimeUnitGuard&) = delete;
    TimeUnitGuard& operator=(const TimeUnitGuard&) = delete;

private:
    TimeUnit m_PreviousUnit;
};

} // namespace ktt::db
