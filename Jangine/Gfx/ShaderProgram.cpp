#include "ShaderProgram.hpp"

#if defined(_MSC_VER)
#pragma warning(push, 0)
#elif defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif

#include <iostream>
#pragma comment(lib, "dxcompiler.lib")

#include <Windows.h>

// Include dxcompiler header from Vulkan SDK
/*#if defined(_WIN32)
#undef _WIN32
#include <dxc/WinAdapter.h>
#define _WIN32
#else
#include <dxc/WinAdapter.h>
#endif*/
#include <dxc/dxcapi.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

std::string wstring_to_utf8(const std::wstring &wstr)
{
    if (wstr.empty())
        return {};
    int size_needed = WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(
        CP_UTF8, 0, wstr.data(), (int)wstr.size(), strTo.data(), size_needed, nullptr, nullptr);
    return strTo;
}

namespace Jangine::Gfx
{
    class IncludeHandler : public IDxcIncludeHandler
    {
    public:
        IncludeHandler(IDxcUtils *dxcUtils) : refCount(1), dxcUtils(dxcUtils) {}
        ~IncludeHandler() = default;

        void AddIncludeSource(const std::string &filename, const std::string &source)
        {
            includeSources[filename] = source;
        }

        // IUnknown methods
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) override
        {
            if (riid == __uuidof(IUnknown) || riid == __uuidof(IDxcIncludeHandler))
            {
                *ppvObject = static_cast<IDxcIncludeHandler *>(this);
                AddRef();
                return S_OK;
            }
            *ppvObject = nullptr;
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef() override
        {
            return ++refCount;
        }

        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG newRefCount = --refCount;
            if (newRefCount == 0)
            {
                delete this;
            }
            return newRefCount;
        }

        // IDxcIncludeHandler method
        HRESULT STDMETHODCALLTYPE LoadSource(LPCWSTR pFilename, IDxcBlob **ppIncludeSource) override
        {
            std::wcout << "IncludeHandler::LoadSource called for file: " << std::wstring(pFilename) << std::endl;

            std::wstring wsFilename(pFilename);
            std::string filename = wstring_to_utf8(wsFilename);

            // filename is a path, we need only the file name part
            std::string fileNameOnly = std::filesystem::path(filename).filename().string();

            auto it = includeSources.find(fileNameOnly);
            if (it != includeSources.end())
            {
                // Found include source in the map
                std::string includeSource = it->second;

                IDxcBlobEncoding *fileContentsBlob = nullptr;

                HRESULT hr = dxcUtils->CreateBlob(
                    includeSource.data(),
                    static_cast<UINT32>(includeSource.size()),
                    CP_UTF8,
                    &fileContentsBlob);

                if (FAILED(hr))
                {
                    return hr;
                }

                *ppIncludeSource = fileContentsBlob;
                return S_OK;
            }
            else
            {
                std::cerr << "Include file not found: " << filename << std::endl;
                *ppIncludeSource = nullptr;
                return E_FAIL;
            }
        }

    private:
        std::unordered_map<std::string, std::string> includeSources;
        std::atomic<ULONG> refCount;
        IDxcUtils *dxcUtils;
    };

    static std::string StageNameToEntrypoint(const std::string &stageName)
    {
        if (stageName == "vertex")
            return "vertex";
        else if (stageName == "fragment" || stageName == "pixel")
            return "fragment";
        else if (stageName == "compute")
            return "compute";
        else if (stageName == "geometry")
            return "geometry";
        else
            throw std::runtime_error("Unsupported shader stage name: " + stageName);
    }

    static std::string StageToProfile(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::VERTEX:
            return "vs_6_0";
        case ShaderStage::FRAGMENT:
            return "ps_6_0";
        case ShaderStage::COMPUTE:
            return "cs_6_0";
        case ShaderStage::GEOMETRY:
            return "gs_6_0";
        default:
            throw std::runtime_error("Unsupported shader stage");
        }
    }

    ShaderCompiler::ShaderCompiler()
    {
        // Create DXC library and compiler
        DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils));
        DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));

        // Create include handler
        includeHandler = new IncludeHandler(utils);
    }

    ShaderCompiler::~ShaderCompiler()
    {
        if (includeHandler)
            includeHandler->Release();
        if (utils)
            utils->Release();
        if (compiler)
            compiler->Release();
    }

    bool_t ShaderCompiler::HasAnyEntrypoint(const std::string_view sourceCode) const
    {
        bool_t found = false;

        std::istringstream sourceReader = std::istringstream(std::string(sourceCode));
        std::string line;
        while (std::getline(sourceReader, line))
        {
            if (line.rfind("[shader", 0) == 0)
            {
                found = true;
                break;
            }
        }
        return found;
    }

    void ShaderCompiler::AddIncludeSource(const std::string &filename, const std::string &source)
    {
        if (includeHandler)
        {
            includeHandler->AddIncludeSource(filename, source);
        }
    }

    std::optional<ShaderProgram> ShaderCompiler::Compile(const std::string_view sourceCode, const std::string_view programName)
    {
        std::vector<std::string> shaderStages;
        std::vector<std::string> shaderStagesEntryPoints;
        bool ignoreThisFile = false;

        std::istringstream sourceReader = std::istringstream(std::string(sourceCode));
        std::string line;
        while (std::getline(sourceReader, line))
        {
            // Trim leading whitespace
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            if (line.rfind("[shader", 0) != 0)
            {
                continue;
            }

            // Find first and second double quote
            auto doubleQuoteIndex = line.find('"');
            if (doubleQuoteIndex == std::string::npos)
            {
                std::cerr << "Error: Invalid shader name in " << programName << std::endl;
                ignoreThisFile = true;
                break;
            }
            auto secondDoubleQuoteIndex = line.find('"', doubleQuoteIndex + 1);
            if (secondDoubleQuoteIndex == std::string::npos)
            {
                std::cerr << "Error: Invalid shader name in " << programName << std::endl;
                ignoreThisFile = true;
                break;
            }

            std::string shaderStageName = line.substr(doubleQuoteIndex + 1, secondDoubleQuoteIndex - doubleQuoteIndex - 1);
            shaderStages.push_back(shaderStageName);

            // Read next line for entry point
            if (!std::getline(sourceReader, line))
            {
                break;
            }

            // Find first space and opening parenthesis
            auto spaceIndex = line.find(' ');
            if (spaceIndex == std::string::npos)
            {
                std::cerr << "Error: Invalid shader entry point in " << programName << std::endl;
                ignoreThisFile = true;
                break;
            }
            auto openingParenthesisIndex = line.find('(', spaceIndex + 1);
            if (openingParenthesisIndex == std::string::npos)
            {
                std::cerr << "Error: Invalid shader entry point in " << programName << std::endl;
                ignoreThisFile = true;
                break;
            }

            std::string shaderEntrypointName = line.substr(spaceIndex + 1, openingParenthesisIndex - spaceIndex - 1);
            shaderEntrypointName.erase(0, shaderEntrypointName.find_first_not_of(" \t\r\n"));
            shaderEntrypointName.erase(shaderEntrypointName.find_last_not_of(" \t\r\n") + 1);

            if (shaderEntrypointName.empty())
            {
                std::cerr << "Error: Invalid shader entry point in " << programName << std::endl;
                ignoreThisFile = true;
                break;
            }

            shaderStagesEntryPoints.push_back(shaderEntrypointName);
            ignoreThisFile = false;
        }

        if (ignoreThisFile)
        {
            return std::nullopt;
        }

        if (shaderStages.empty())
        {
            std::cerr << "Error: No shader stages specified in " << programName << std::endl;
            return std::nullopt;
        }

        if (!shaderStagesEntryPoints.empty() && shaderStagesEntryPoints.size() != shaderStages.size())
        {
            std::cerr << "Error: Number of shader stage entry points does not match the number of shader stages in " << programName << std::endl;
            return std::nullopt;
        }

        // If no shader stage entry points were found, use default entry points
        if (shaderStagesEntryPoints.empty())
        {
            for (const auto &stageName : shaderStages)
            {
                shaderStagesEntryPoints.push_back(StageNameToEntrypoint(stageName));
            }
        }

        std::vector<ShaderProgram::Stage> stages;
        for (size_t i = 0; i < shaderStages.size(); ++i)
        {
            ShaderStage stage = ShaderStage::VERTEX;
            if (shaderStages[i] == "vertex")
                stage = ShaderStage::VERTEX;
            else if (shaderStages[i] == "fragment" || shaderStages[i] == "pixel")
                stage = ShaderStage::FRAGMENT;
            else if (shaderStages[i] == "compute")
                stage = ShaderStage::COMPUTE;
            else if (shaderStages[i] == "geometry")
                stage = ShaderStage::GEOMETRY;
            else
            {
                std::cerr << "Error: Unsupported shader stage: " << shaderStages[i] << std::endl;
                continue;
            }

            auto compiledStage = CompileStage(sourceCode, stage, shaderStagesEntryPoints[i]);
            if (!compiledStage)
            {
                return std::nullopt;
            }
            stages.push_back(*compiledStage);
        }

        // Create ShaderProgram with the compiled stages
        ShaderProgram shaderProgram(std::string(programName), std::move(stages));
        return shaderProgram;
    }

    std::optional<ShaderProgram::Stage> ShaderCompiler::CompileStage(const std::string_view sourceCode, ShaderStage stage, const std::string_view entryPoint)
    {
        // Create blob from source code
        DxcBuffer sourceBuffer = {
            .Ptr = static_cast<const void *>(sourceCode.data()),
            .Size = static_cast<UINT32>(sourceCode.size()),
            .Encoding = CP_UTF8};

        // Set up compile arguments
        LPCWSTR args[] = {
            L"-spirv",
            L"-O3"};

        std::string shaderProfile = StageToProfile(stage);

        IDxcCompilerArgs *argsPtr = nullptr;
        utils->BuildArguments(
            nullptr,
            std::wstring(entryPoint.begin(), entryPoint.end()).c_str(),
            std::wstring(shaderProfile.begin(), shaderProfile.end()).c_str(),
            args,
            _countof(args),
            nullptr,
            0,
            &argsPtr);

        // Compile
        IDxcResult *result = nullptr;
        compiler->Compile(
            &sourceBuffer,
            argsPtr->GetArguments(),
            argsPtr->GetCount(),
            includeHandler,
            __uuidof(IDxcResult),
            reinterpret_cast<void **>(&result));

        // Check result
        HRESULT hr = S_OK;
        result->GetStatus(&hr);
        if (FAILED(hr))
        {
            IDxcBlobEncoding *errors = nullptr;
            result->GetErrorBuffer(&errors);
            if (errors)
            {
                if (errors->GetBufferSize() > 0)
                {
                    std::cerr << std::string(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize())
                              << std::endl;
                }
                errors->Release();
            }

            return std::nullopt;
        }

        // Get compiled SPIR-V
        IDxcBlob *spirvBlob = nullptr;
        result->GetResult(&spirvBlob);

        std::vector<uint8_t> bytecode(
            static_cast<uint8_t *>(spirvBlob->GetBufferPointer()),
            static_cast<uint8_t *>(spirvBlob->GetBufferPointer()) + spirvBlob->GetBufferSize());

        ShaderProgram::Stage shaderStage(
            stage,
            std::string(entryPoint),
            std::move(bytecode));

        // Release resources
        if (spirvBlob)
            spirvBlob->Release();
        if (result)
            result->Release();

        return shaderStage;
    }
}