#pragma once
#include <cstddef>
#include <vector>

#include <Kernel/KernelDefinition.h>
#include <Kernel/KernelParameter.h>
#include <Kernel/KernelConstraint/KernelConstraint.h>

namespace ktt
{

class FingerPrintUtility
{
public:
    static std::size_t GetFingerprintOfParameters(const std::set<KernelParameter> &params);
    static std::tuple<size_t, bool> GetFingerprintOfConstraints(const std::vector<const KernelConstraint *> &constraints);
    static std::size_t GetFingerPrintOfDefinitions(const std::vector<const KernelDefinition *> &definitions);

private:
    static std::size_t HashFunction(std::size_t base, std::size_t value);
};

} // namespace ktt
