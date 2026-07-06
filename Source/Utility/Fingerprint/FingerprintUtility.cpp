
#include <set>

#include <Utility/Fingerprint/FingerprintUtility.h>

namespace ktt
{

std::size_t FingerprintUtility::GetFingerprintOfParameters(const std::set<KernelParameter> &params)
{
    std::size_t base = 0;
    std::vector parameters(params.begin(), params.end());

    std::sort(parameters.begin(), parameters.end());

    for (const auto &parameter : parameters)
    {
        const std::size_t paramHash = std::hash<std::string>{}(parameter.GetName());
        base = HashFunction(base, paramHash);
        // for (const auto &value : parameter.GetValues())
        // {
        //     const std::size_t paramValHash = std::hash<std::string>{}(ParameterValueToString(value));
        //     base = HashFunction(base, paramValHash);
        // }
    }

    return base;
}

std::size_t FingerprintUtility::GetFingerprintOfDefinitions(const std::vector<const KernelDefinition *> &definitions)
{
    std::size_t base = 0;
    for (const auto *definition : definitions)
    {
        const std::size_t defHash = std::hash<std::string>{}(definition->GetSource());
        base = HashFunction(base, defHash);
    }
    return base;
}

std::size_t FingerprintUtility::HashFunction(std::size_t base, std::size_t value)
{
    return base ^ (value + 0x9e3779b97f4a7c15 + (base << 6) + (base >> 2));
}

} // namespace ktt
