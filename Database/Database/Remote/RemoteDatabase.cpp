#include <utility>

#include <Api/KttException.h>
#include <Database/Remote/Http/CurlHttpClient.h>
#include <Database/Remote/RemoteClient.h>
#include <Database/Remote/RemoteDatabase.h>

namespace ktt::db
{

RemoteDatabase::RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options) :
    RemoteDatabase(std::move(baseUrl), std::move(apiKey), options, std::make_shared<CurlHttpClient>(options))
{}

RemoteDatabase::RemoteDatabase(std::string baseUrl, std::string apiKey, RemoteDatabaseOptions options,
    std::shared_ptr<HttpClient> httpClient) :
    m_Client(std::make_unique<RemoteClient>(std::move(baseUrl), std::move(apiKey), options.maxRetries,
        std::move(httpClient)))
{}

RemoteDatabase::~RemoteDatabase() = default;

// The endpoint methods are implemented by work package C3 (cpp-requirements.md R4).

void RemoteDatabase::SaveResults(const TuningInfo &, std::vector<KernelResult>, SaveOptions) const
{
    throw KttException("RemoteDatabase::SaveResults is not implemented yet");
}

std::vector<KernelResult> RemoteDatabase::SimpleGetBestResults(const TuningInfo &, uint32_t) const
{
    throw KttException("RemoteDatabase::SimpleGetBestResults is not implemented yet");
}

std::vector<KernelResult> RemoteDatabase::GetBestResults(const GetBestResultsQuery &) const
{
    throw KttException("RemoteDatabase::GetBestResults is not implemented yet");
}

std::optional<SourceStats> RemoteDatabase::GetStatsForSource(size_t) const
{
    throw KttException("RemoteDatabase::GetStatsForSource is not implemented yet");
}

} // namespace ktt::db
