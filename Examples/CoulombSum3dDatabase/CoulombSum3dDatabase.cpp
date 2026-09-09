#include "../ExampleReferenceComputation.h"
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace std;

struct CoulombSum3dDatabaseConfiguration : ExampleConfiguration
{
    uint64_t gridWidth = 128;
    uint64_t gridHeight = 128;
    uint64_t gridDepth = 128;
    uint64_t numberOfAtoms = 256;
    float gridSpacing = 0.5f;
    bool sepCompTuning = false;

    // Database file whose runs are synced into the local KTT database before tuning.
    // Empty by default; set via --db, in which case that file is synced.
    string dbSyncPath = "";
};

void SetUpCoulombSum3dDatabaseOptions(vector<CliOption> &options, CoulombSum3dDatabaseConfiguration &config)
{
    options.emplace_back([&config](const vector<string> &args) {
        config.gridWidth = stoul(args[0]);
    }, "--gridWidth", "Set the grid width (expects int), scaled proportionally with"
    " cbrt of problemSize", "<width>", 1);
    options.emplace_back([&config](const vector<string> &args) {
        config.gridHeight = stoul(args[0]);
    }, "--gridHeight", "Set the grid height (expects int), scaled proportionally with"
    " cbrt of problemSize", "<height>", 1);
    options.emplace_back([&config](const vector<string> &args) {
        config.gridDepth = stoul(args[0]);
    }, "--gridDepth", "Set the grid depth (expects int), scaled proportionally with"
    " cbrt of problemSize", "<depth>", 1);
    options.emplace_back([&config](const vector<string> &args) {
        config.numberOfAtoms = stoul(args[0]);
    }, "--atoms", "Set the number of atoms (expects int)", "<count>", 1);
    options.emplace_back([&config](const vector<string> &args) {
        config.gridSpacing = stof(args[0]);
    }, "--spacing", "Set the grid spacing (expects float)", "<spacing>", 1);
    options.emplace_back([&config](const vector<string> &) {
        config.sepCompTuning = true;
    }, "--sepCompTuning", "Enable separate compiler parameter tuning.");
    options.emplace_back([&config](const vector<string> &args) {
        config.dbSyncPath = args[0];
    }, "--db", "Path to a KTT database file to sync into the local database before tuning",
    "<path>", 1);
}

CoulombSum3dDatabaseConfiguration CoulombSum3dDatabaseProcessInput(int argc, char **argv)
{
    CoulombSum3dDatabaseConfiguration config;
    vector<CliOption> options;
    SetUpCommonOptions(options, &config);
    SetUpCoulombSum3dDatabaseOptions(options, config);

    IterateArguments(argc, argv, options);

    return config;
}

class CoulombSum3dDatabase : public ExampleReferenceComputation {
protected:
    CoulombSum3dDatabase(shared_ptr<CoulombSum3dDatabaseConfiguration> config, int defaultProblemSize,
                         string exampleFolderPath, string defaultKernelFileBaseName) :
        ExampleReferenceComputation(config, defaultProblemSize, exampleFolderPath,
                                    defaultKernelFileBaseName),
        m_gridWidth(config->gridWidth * static_cast<size_t>(cbrt(m_problemSize))),
        m_gridHeight(config->gridHeight * static_cast<size_t>(cbrt(m_problemSize))),
        m_gridDepth(config->gridDepth * static_cast<size_t>(cbrt(m_problemSize))),
        m_ndRangeDimensions(m_gridWidth, m_gridHeight, m_gridDepth),
        m_workGroupDimensions{1, 1, 1},
        m_numberOfAtoms(config->numberOfAtoms),
        m_gridSpacing(config->gridSpacing),
        m_sepCompTuning(config->sepCompTuning),
        m_dbSyncPath(config->dbSyncPath)
    {
    }

public:
    static unique_ptr<CoulombSum3dDatabase> Create(
        int argc, char** argv,
        int defaultProblemSize,
        string exampleFolderPath,
        string defaultKernelFileBaseName
    ) {
        auto config = make_shared<CoulombSum3dDatabaseConfiguration>(CoulombSum3dDatabaseProcessInput(argc, argv));
        unique_ptr<CoulombSum3dDatabase> ex(new CoulombSum3dDatabase(config, defaultProblemSize, exampleFolderPath, defaultKernelFileBaseName));
        ex->PostInitialize();
        return ex;
    }

    // Custom offline run that keeps the tuning results in hand so they can be
    // pushed to / loaded from the KTT database directly. Structurally this mirrors
    // CoulombSum3d::Run() (ExampleBase offline tuning + optional separate compiler
    // tuning), with the database interaction woven around the Tune() call.
    void Run()
    {
        /**
         * --- Database integration: prep ---
         */
        const ktt::db::Database db;

        // Sync the database file passed via --db into the local KTT database.
        // Skipped when --db was not provided.
        if (!m_dbSyncPath.empty())
        {
            db.SyncFromFile(m_dbSyncPath);
        }

        const auto tuningInfo = m_tuner.GetDatabaseTuningInfo(m_kernel);

        /**
         * --- Database integration: load previously stored best results ---
         */
        const auto simpleResults = db.SimpleGetBestResults(tuningInfo, 50);

        // Advanced query variant: same tuning space, restricted to CUDA devices.
        const ktt::db::GetBestResultsQuery query{
            tuningInfo.spaceInfo,
            std::function<bool(const ktt::db::DeviceInfo &)>([](const ktt::db::DeviceInfo &device) {
                return device.computeApi == ktt::ComputeApi::CUDA;
            }),
            std::nullopt, // no input filter
            50
        };
        const auto queryResults = db.GetBestResults(query);

        cout << "Loaded " << simpleResults.size() << " best result(s) from the database "
             << "(" << queryResults.size() << " via CUDA-filtered query)." << endl;

        /**
         * --- Database integration: run the best previously known configuration ---
         */
        if (!simpleResults.empty())
        {
            const auto bestConfig = simpleResults[0].GetConfiguration();
            const auto bestResult = m_tuner.Run(m_kernel, bestConfig, {});
            cout << "Re-ran best known configuration: " << bestResult.GetTotalDuration()
                 << " ns" << endl;
        }

        // --- Offline tuning (driven here so we hold the results directly) ---
        const auto results = m_tuner.Tune(m_kernel, std::move(m_config->stopCondition), m_config->preciseParams);

        m_tuner.SaveResults(results, "Output", ktt::OutputFormat::JSON);

        /**
         * --- Database integration: save this run's results ---
         */
        auto save = m_tuner.GetDatabaseTuningInfo(m_kernel);
        save.inputData = "atoms=" + std::to_string(m_numberOfAtoms)
                       + ";gridSize=" + std::to_string(m_gridWidth);
        db.SaveResults(save, results, {ktt::OutputFormat::JSON, 2});

        // --- Optional: tune compiler options on top of the best kernel config ---
        if (!m_sepCompTuning) return;
        auto bestConfig = m_tuner.GetBestConfiguration(m_kernel);
        if (!bestConfig.IsValid()) {
            cout << "No valid configuration was found. Skippin separate compiler parameter tuning.\n";
            return;
        }
        std::cout << "\nTuning compiler options on top of best kernel configuration..." << std::endl;
        const auto optResults = m_tuner.TuneOptions(m_kernel, bestConfig);
        m_tuner.SaveResults(optResults, "CoulombSumOptionsOutput", ktt::OutputFormat::JSON);
    }

protected:
    friend ExampleBase;

    size_t m_gridWidth;
    size_t m_gridHeight;
    size_t m_gridDepth;

    // Total NDRange size matches number of grid points
    const ktt::DimensionVector m_ndRangeDimensions;
    const ktt::DimensionVector m_workGroupDimensions;
    const ktt::DimensionVector m_referenceWorkGroupDimensions{16, 16, 16};

    const int m_numberOfAtoms;
    const float m_gridSpacing;

    const bool m_sepCompTuning;

    const string m_dbSyncPath;

    vector<float> m_atomInfo;
    vector<float> m_atomInfoX;
    vector<float> m_atomInfoY;
    vector<float> m_atomInfoZ;
    vector<float> m_atomInfoW;
    vector<float> m_energyGrid;

    ktt::ArgumentId m_atomInfoId;
    ktt::ArgumentId m_atomInfoXId;
    ktt::ArgumentId m_atomInfoYId;
    ktt::ArgumentId m_atomInfoZId;
    ktt::ArgumentId m_atomInfoWId;
    ktt::ArgumentId m_numberOfAtomsId;
    ktt::ArgumentId m_gridSpacingId;
    ktt::ArgumentId m_gridDimId;
    ktt::ArgumentId m_energyGridId;

    void AddCompilerParameter(const string &name, const vector<string> &values = {})
    {
        if (m_sepCompTuning) m_tuner.AddSeparateCompilerParameter(m_kernel, name, values);
        else m_tuner.AddCompilerParameter(m_kernel, name, values);
    }

    void InitData() override
    {
        // Declare data variables
        const size_t numberOfGridPoints = m_gridWidth * m_gridHeight * m_gridDepth;
        m_atomInfo.resize(4 * m_numberOfAtoms);
        m_atomInfoX.resize(m_numberOfAtoms);
        m_atomInfoY.resize(m_numberOfAtoms);
        m_atomInfoZ.resize(m_numberOfAtoms);
        m_atomInfoW.resize(m_numberOfAtoms);
        m_energyGrid.resize(numberOfGridPoints, 0.0f);

        FillBuffers<float>({&m_atomInfoX, &m_atomInfoY, &m_atomInfoZ}, 0.0f, 20.0f);
        FillBuffers<float>({&m_atomInfoW}, 0.0f, 0.5f);

        for (int i = 0; i < m_numberOfAtoms; ++i)
        {
            m_atomInfo[4 * i] = m_atomInfoX[i];
            m_atomInfo[4 * i + 1] = m_atomInfoY[i];
            m_atomInfo[4 * i + 2] = m_atomInfoZ[i];
            m_atomInfo[4 * i + 3] = m_atomInfoW[i];
        }
    }

    void InitKernel() override
    {
        // Add all kernel arguments
        m_atomInfoId = m_tuner.AddArgumentVector(m_atomInfo, ktt::ArgumentAccessType::ReadOnly);
        m_atomInfoXId = m_tuner.AddArgumentVector(m_atomInfoX, ktt::ArgumentAccessType::ReadOnly);
        m_atomInfoYId = m_tuner.AddArgumentVector(m_atomInfoY, ktt::ArgumentAccessType::ReadOnly);
        m_atomInfoZId = m_tuner.AddArgumentVector(m_atomInfoZ, ktt::ArgumentAccessType::ReadOnly);
        m_atomInfoWId = m_tuner.AddArgumentVector(m_atomInfoW, ktt::ArgumentAccessType::ReadOnly);
        m_numberOfAtomsId = m_tuner.AddArgumentScalar(m_numberOfAtoms);
        m_gridSpacingId = m_tuner.AddArgumentScalar(m_gridSpacing);
        m_gridDimId = m_tuner.AddArgumentScalar(static_cast<int>(m_gridWidth));
        m_energyGridId = m_tuner.AddArgumentVector(m_energyGrid, ktt::ArgumentAccessType::WriteOnly);

        // Configure main kernel
        InitKernelDefault("directCoulombSum", "CoulombSum", m_ndRangeDimensions,
            {m_atomInfoId, m_atomInfoXId, m_atomInfoYId, m_atomInfoZId, m_atomInfoWId,
             m_numberOfAtomsId, m_gridSpacingId, m_gridDimId, m_energyGridId});
    }

    void InitTuningSpace() override
    {
        UseFastMath();
        UseOpenMP();

        if (m_computeApi == ktt::ComputeApi::OpenCL || m_computeApi == ktt::ComputeApi::CUDA)
        {
            m_tuner.AddParameter(m_kernel, "WORK_GROUP_SIZE_X", vector<uint64_t>{16, 32});
            m_tuner.AddThreadModifier(m_kernel, {m_definition}, ktt::ModifierType::Local, ktt::ModifierDimension::X, "WORK_GROUP_SIZE_X",
                ktt::ModifierAction::Multiply);
            m_tuner.AddThreadModifier(m_kernel, {m_definition}, ktt::ModifierType::Global, ktt::ModifierDimension::X, "WORK_GROUP_SIZE_X",
                ktt::ModifierAction::DivideCeil);

            m_tuner.AddParameter(m_kernel, "WORK_GROUP_SIZE_Y", vector<uint64_t>{1, 2, 4, 8});
            m_tuner.AddThreadModifier(m_kernel, {m_definition}, ktt::ModifierType::Local, ktt::ModifierDimension::Y, "WORK_GROUP_SIZE_Y",
                ktt::ModifierAction::Multiply);
            m_tuner.AddThreadModifier(m_kernel, {m_definition}, ktt::ModifierType::Global, ktt::ModifierDimension::Y, "WORK_GROUP_SIZE_Y",
                ktt::ModifierAction::DivideCeil);

            m_tuner.AddParameter(m_kernel, "WORK_GROUP_SIZE_Z", vector<uint64_t>{1});
            m_tuner.AddParameter(m_kernel, "Z_ITERATIONS", vector<uint64_t>{1, 2, 4, 8, 16, 32});
            m_tuner.AddThreadModifier(m_kernel, {m_definition}, ktt::ModifierType::Global, ktt::ModifierDimension::Z, "Z_ITERATIONS",
                ktt::ModifierAction::DivideCeil);

            m_tuner.AddParameter(m_kernel, "INNER_UNROLL_FACTOR", vector<uint64_t>{0, 1, 2, 4, 8, 16, 32});

            auto lt = [](const vector<uint64_t>& vector) {return vector.at(0) < vector.at(1);};
            m_tuner.AddConstraint(m_kernel, {"INNER_UNROLL_FACTOR", "Z_ITERATIONS"}, lt);
            auto par = [](const vector<uint64_t>& vector) {return vector.at(0) * vector.at(1) >= 64;};
            m_tuner.AddConstraint(m_kernel, {"WORK_GROUP_SIZE_X", "WORK_GROUP_SIZE_Y"}, par);

            if (m_computeApi == ktt::ComputeApi::OpenCL)
            {
                m_tuner.AddParameter(m_kernel, "USE_CONSTANT_MEMORY", vector<uint64_t>{0, 1});
                m_tuner.AddParameter(m_kernel, "USE_SOA", vector<uint64_t>{0, 1});
                m_tuner.AddParameter(m_kernel, "VECTOR_SIZE", vector<uint64_t>{1, 2 , 4, 8, 16});

                auto vec = [](const vector<uint64_t>& vector) {return vector.at(0) || vector.at(1) == 1; };
                m_tuner.AddConstraint(m_kernel, {"USE_SOA", "VECTOR_SIZE"}, vec);

                // Individual math optimization flags replacing the global -cl-fast-relaxed-math.
                // -cl-fast-relaxed-math ≈ -cl-finite-math-only + -cl-unsafe-math-optimizations,
                // where -cl-unsafe-math-optimizations implies -cl-mad-enable, -cl-no-signed-zeros,
                // and -cl-denorms-are-zero. Tuning them individually reveals which subset is sufficient.
                AddCompilerParameter("-cl-mad-enable");
                AddCompilerParameter("-cl-no-signed-zeros");
                AddCompilerParameter("-cl-finite-math-only");
                AddCompilerParameter("-cl-denorms-are-zero");
            }
            else // CUDA
            {
                m_tuner.AddParameter(m_kernel, "USE_CONSTANT_MEMORY", vector<uint64_t>{0});
                m_tuner.AddParameter(m_kernel, "USE_SOA", vector<uint64_t>{0, 1});
                m_tuner.AddParameter(m_kernel, "VECTOR_SIZE", vector<uint64_t>{1});

                // Register count limit: trades register file pressure for higher occupancy.
                // Relevant here because energyValue[Z_ITERATIONS] can hold up to 32 floats,
                // creating high register pressure at large Z_ITERATIONS values.
                // 0 = unlimited (compiler decides), others force a ceiling.
                AddCompilerParameter("--maxrregcount ", {"0", "32", "40", "48", "64"});
            }
        }
        else // CPP
        {
            m_tuner.AddParameter(m_kernel, "OMP_COLLAPSE", vector<uint64_t>{0, 1});
            m_tuner.AddParameter(m_kernel, "OMP_SCHEDULING", vector<uint64_t>{0, 1, 2});
            m_tuner.AddParameter(m_kernel, "OMP_SCHED_CHUNK", vector<uint64_t>{2, 4, 8, 16, 32, 64, 128});
            m_tuner.AddParameter(m_kernel, "TILE", vector<uint64_t>{0, 8, 16, 32, 64});

            AddCompilerParameter("-ffast-math");
            AddCompilerParameter("-O", {"1", "2", "3"});
            AddCompilerParameter("-funroll-loops");
            // Auto-vectorization of the inner atom loop (SIMD via AVX2/AVX512 enabled by -march=native).
            AddCompilerParameter("-ftree-vectorize");
            // Software prefetching of the atom SoA arrays in the inner loop (GCC-specific).
            AddCompilerParameter("-fprefetch-loop-arrays");
            // Allows the compiler to skip errno updates in math functions (lighter than -ffast-math,
            // enables vectorization of sqrtf without full unsafe-math semantics).
            AddCompilerParameter("-fno-math-errno");

            auto schedchunk = [](const vector<uint64_t>& vector) {return vector.at(0) == 2 || vector.at(1) == 2; };
            m_tuner.AddConstraint(m_kernel, {"OMP_SCHEDULING", "OMP_SCHED_CHUNK"}, schedchunk);
        }
    }

    void InitReference() override
    {
        if (!m_config->rapidTest)
        {
            //TODO: this is temporary hack, there should be composition of zeroizing and Coulomb kernel,
            // otherwise, multiple profiling runs corrupt results
            m_tuner.SetReferenceComputation(m_energyGridId, [this](void* buffer) {
                float* grid = static_cast<float*>(buffer);

                #pragma omp parallel for
                for (size_t z = 0; z < m_gridDepth; z++)
                    for (size_t y = 0; y < m_gridHeight; y++)
                        for (size_t x = 0; x < m_gridWidth; x++)
                        {
                            float e = 0.0f;
                            float gx = static_cast<float>(x) * m_gridSpacing;
                            float gy = static_cast<float>(y) * m_gridSpacing;
                            float gz = static_cast<float>(z) * m_gridSpacing;
                            for (int a = 0; a < m_numberOfAtoms; a++)
                                e += m_atomInfoW[a] * (1.0f / sqrtf((m_atomInfoX[a] - gx) * (m_atomInfoX[a] - gx) +
                                    (m_atomInfoY[a] - gy) * (m_atomInfoY[a] - gy) +
                                    (m_atomInfoZ[a] - gz) * (m_atomInfoZ[a] - gz)));
                            grid[z * m_gridWidth * m_gridHeight + y * m_gridWidth + x] = e;
                        }
            });
            m_tuner.SetValidationMethod(ktt::ValidationMethod::SideBySideComparison, 0.01);
        }
    }
};

int main(int argc, char **argv)
{
    unique_ptr<CoulombSum3dDatabase> coulombSum3dDatabase =
        CoulombSum3dDatabase::Create(argc, argv, 8, "Examples/CoulombSum3dDatabase", "CoulombSum3dDatabase");
    coulombSum3dDatabase->Run();

    return 0;
}
