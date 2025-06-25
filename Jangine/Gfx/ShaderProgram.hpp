#include "Shared.hpp"

#include <sstream>
#include <fstream>
#include <nlohmann/json.hpp>

struct IDxcUtils;
struct IDxcCompiler3;

namespace Jangine::Gfx
{
    struct ShaderProgram
    {
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
        struct ShaderPackage
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

            static void SerializeToHeader(
                const std::unordered_map<std::string, ShaderProgram> &shaderPrograms,
                const std::string &headerFileName,
                const std::string &arrayName,
                const std::string &namespaceName)
            {
                std::ostringstream oss(std::ios::binary);
                Serialize(shaderPrograms, oss);
                std::string data = oss.str();

                std::ofstream out(headerFileName, std::ios::out | std::ios::trunc);
                if (!out)
                    throw std::runtime_error("Failed to open header file for writing: " + headerFileName);

                out << "// This file was autogenerated.\n";
                out << "#pragma once\n\n";
                out << "#include <cstdint>\n#include <cstddef>\n\n";
                out << "namespace " << namespaceName << " {\n\n";
                out << "constexpr std::size_t " << arrayName << "_size = " << data.size() << ";\n";
                out << "constexpr uint8_t " << arrayName << "[" << data.size() << "] = {";

                for (size_t i = 0; i < data.size(); ++i)
                {
                    if (i % 16 == 0)
                    {
                        out << "\n    ";
                    }
                    out << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
                        << (static_cast<uint32_t>(static_cast<uint8_t>(data[i])));
                    if (i + 1 != data.size())
                        out << ", ";
                }
                out << std::dec; // restore decimal output
                out << "\n};\n";
                out << "\n} // namespace " << namespaceName << "\n";
                out.close();
            }
        };
    }

    class ShaderCompiler
    {
    public:
        ShaderCompiler();
        ~ShaderCompiler();

        ShaderProgram Compile(const std::string_view sourceCode, const std::string_view programName);
        ShaderProgram::Stage CompileStage(const std::string_view sourceCode, ShaderStage stage, const std::string_view entryPoint);

    private:
        IDxcUtils *utils = nullptr;
        IDxcCompiler3 *compiler = nullptr;
    };
}