#include "Shared.hpp"

#include <nlohmann/json.hpp>

struct IDxcUtils;
struct IDxcCompiler3;

namespace Jangine::Gfx
{
    // Complete shader pipeline, e.g. vertex + fragment, etc.
    struct ShaderProgram
    {
        // Single shader stage, e.g. vertex, fragment, compute, etc.
        struct Stage
        {
            ShaderStage stage;
            std::string entryPoint;
            std::vector<uint8_t> bytecode;

            explicit Stage() = default;
            explicit Stage(ShaderStage shaderStage, const std::string &entry, std::vector<uint8_t> &&code)
                : stage(shaderStage), entryPoint(entry), bytecode(std::move(code)) {}
        };

        std::string name;
        std::vector<Stage> stages;

        explicit ShaderProgram() = default;
        explicit ShaderProgram(const std::string &shaderName, std::vector<Stage> &&shaderStages)
            : name(shaderName), stages(std::move(shaderStages)) {}
    };

    // Compiles HLSL shaders to SPIR-V bytecode using the DirectX Shader Compiler (DXC) API.
    class ShaderCompiler
    {
    public:
        ShaderCompiler();
        ~ShaderCompiler();

        ShaderCompiler(const ShaderCompiler &) = delete;
        ShaderCompiler &operator=(const ShaderCompiler &) = delete;
        ShaderCompiler(ShaderCompiler &&) = delete;
        ShaderCompiler &operator=(ShaderCompiler &&) = delete;

        [[nodiscard]] ShaderProgram Compile(const std::string_view sourceCode, const std::string_view programName);
        [[nodiscard]] ShaderProgram::Stage CompileStage(const std::string_view sourceCode, ShaderStage stage, const std::string_view entryPoint);

    private:
        IDxcUtils *utils = nullptr;
        IDxcCompiler3 *compiler = nullptr;
    };

    inline void to_json(nlohmann::json &j, const ShaderProgram::Stage &s)
    {
        j = nlohmann::json{
            {"Stage", s.stage},
            {"EntryPoint", s.entryPoint},
            {"Bytecode", s.bytecode}};
    }

    inline void to_json(nlohmann::json &j, const ShaderProgram &p)
    {
        j = nlohmann::json{
            {"Name", p.name},
            {"Stages", p.stages}};
    }

    namespace IO
    {
        // Binary representation of one or more shader programs.
        // JSON metadata (names, data offset, etc.) and bytecode.
        class ShaderPackage
        {
            struct StageDefinition
            {
                uint32_t stage;
                std::string entryPoint;
                int64_t offset;
                int64_t size;

                explicit StageDefinition() = default;
                explicit StageDefinition(uint32_t shaderStage, const std::string &entry, int64_t off, int64_t sz)
                    : stage(shaderStage), entryPoint(entry), offset(off), size(sz) {}
            };

            struct ShaderDefinition
            {
                std::string name;
                std::vector<StageDefinition> stages;

                explicit ShaderDefinition() = default;
                explicit ShaderDefinition(const std::string &shaderName, std::vector<StageDefinition> &&shaderStages)
                    : name(shaderName), stages(std::move(shaderStages)) {}
            };

            std::unordered_map<std::string, ShaderDefinition> pipelines;

            friend void to_json(nlohmann::json &j, const StageDefinition &s)
            {
                j = nlohmann::json{
                    {"Stage", s.stage},
                    {"EntryPoint", s.entryPoint},
                    {"Offset", s.offset},
                    {"Size", s.size}};
            }

            friend void to_json(nlohmann::json &j, const ShaderDefinition &p)
            {
                j = nlohmann::json{
                    {"Name", p.name},
                    {"Stages", p.stages}};
            }

            friend void to_json(nlohmann::json &j, const ShaderPackage &pkg)
            {
                j = nlohmann::json{
                    {"Pipelines", pkg.pipelines}};
            }

            friend void from_json(const nlohmann::json &j, StageDefinition &s)
            {
                s.stage = j.at("Stage").get<uint32_t>();
                s.entryPoint = j.at("EntryPoint").get<std::string>();
                s.offset = j.at("Offset").get<int64_t>();
                s.size = j.at("Size").get<int64_t>();
            }

            friend void from_json(const nlohmann::json &j, ShaderDefinition &p)
            {
                p.name = j.at("Name").get<std::string>();
                p.stages = j.at("Stages").get<std::vector<StageDefinition>>();
            }

            friend void from_json(const nlohmann::json &j, ShaderPackage &pkg)
            {
                pkg.pipelines.clear();
                for (auto it = j.at("Pipelines").begin(); it != j.at("Pipelines").end(); ++it)
                {
                    pkg.pipelines[it.key()] = it.value().get<ShaderDefinition>();
                }
            }

        public:
            static std::unordered_map<std::string, ShaderProgram> Deserialize(std::istream &stream)
            {
                auto read_u32 = [&stream]()
                {
                    uint32_t val;
                    stream.read(reinterpret_cast<char *>(&val), sizeof(val));
                    return val;
                };

                // Json chunk
                uint32_t jsonChunkLength = read_u32();
                uint32_t jsonChunkType = read_u32();
                if (jsonChunkType != 1)
                    throw std::runtime_error("Invalid json chunk type.");

                std::vector<char> jsonChunkData(jsonChunkLength);
                stream.read(jsonChunkData.data(), jsonChunkLength);

                // Bytecode chunk
                uint32_t bytecodeChunkLength = read_u32();
                uint32_t bytecodeChunkType = read_u32();
                if (bytecodeChunkType != 2)
                    throw std::runtime_error("Invalid bytecode chunk type.");

                std::vector<uint8_t> packageBytecode(bytecodeChunkLength);
                stream.read(reinterpret_cast<char *>(packageBytecode.data()), bytecodeChunkLength);

                // Parse JSON
                nlohmann::json j = nlohmann::json::parse(jsonChunkData.begin(), jsonChunkData.end());
                ShaderPackage packageDefinition = j.get<ShaderPackage>();

                std::unordered_map<std::string, ShaderProgram> shaderPrograms;
                for (const auto &[shaderProgramName, shaderProgramDefinition] : packageDefinition.pipelines)
                {
                    std::vector<ShaderProgram::Stage> stages;
                    for (const auto &stageDefinition : shaderProgramDefinition.stages)
                    {
                        std::vector<uint8_t> stageBytecode(stageDefinition.size);
                        std::copy_n(
                            packageBytecode.begin() + stageDefinition.offset,
                            stageDefinition.size,
                            stageBytecode.begin());
                        stages.emplace_back(
                            static_cast<ShaderStage>(stageDefinition.stage),
                            stageDefinition.entryPoint,
                            std::move(stageBytecode));
                    }
                    shaderPrograms.emplace(
                        shaderProgramName,
                        ShaderProgram(shaderProgramName, std::move(stages)));
                }
                return shaderPrograms;
            }

            static void Serialize(
                const std::unordered_map<std::string, ShaderProgram> &shaderPrograms,
                std::ostream &stream)
            {
                ShaderPackage package;
                int64_t currentOffset = 0;
                std::vector<uint8_t> allBytecode;

                for (const auto &[name, pipeline] : shaderPrograms)
                {
                    ShaderDefinition shaderDef;
                    shaderDef.name = pipeline.name;
                    for (const auto &stage : pipeline.stages)
                    {
                        StageDefinition stageDef;
                        stageDef.stage = static_cast<uint32_t>(stage.stage);
                        stageDef.entryPoint = stage.entryPoint;
                        stageDef.offset = currentOffset;
                        stageDef.size = static_cast<int64_t>(stage.bytecode.size());

                        allBytecode.insert(allBytecode.end(), stage.bytecode.begin(), stage.bytecode.end());
                        currentOffset += stageDef.size;

                        shaderDef.stages.push_back(stageDef);
                    }
                    package.pipelines[name] = std::move(shaderDef);
                }

                // Serialize JSON metadata
                nlohmann::json j = package;
                std::string jsonStr = j.dump();
                uint32_t jsonChunkLength = static_cast<uint32_t>(jsonStr.size());
                uint32_t jsonChunkType = 1;

                // Write JSON chunk
                stream.write(reinterpret_cast<const char *>(&jsonChunkLength), sizeof(jsonChunkLength));
                stream.write(reinterpret_cast<const char *>(&jsonChunkType), sizeof(jsonChunkType));
                stream.write(jsonStr.data(), jsonStr.size());

                // Write bytecode chunk
                uint32_t bytecodeChunkLength = static_cast<uint32_t>(allBytecode.size());
                uint32_t bytecodeChunkType = 2;
                stream.write(reinterpret_cast<const char *>(&bytecodeChunkLength), sizeof(bytecodeChunkLength));
                stream.write(reinterpret_cast<const char *>(&bytecodeChunkType), sizeof(bytecodeChunkType));
                stream.write(reinterpret_cast<const char *>(allBytecode.data()), allBytecode.size());
            }
        };
    }
}