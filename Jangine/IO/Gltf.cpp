#include "Gltf.hpp"

namespace Jangine::IO::Gltf
{
    Status LoadFromFile(const std::string &filename, Model &model)
    {
        std::ifstream fileStream(filename, std::ios::binary);
        if (!fileStream)
        {
            return Status::FAILURE;
        }

        auto read_u32 = [&fileStream]()
        {
            uint32_t val;
            fileStream.read(reinterpret_cast<char *>(&val), sizeof(val));
            return val;
        };

        uint32_t magic = read_u32();
        if (magic != 0x46546C67) // "glTF"
        {
            return Status::FAILURE;
        }

        uint32_t version = read_u32();
        if (version != 2)
        {
            return Status::FAILURE;
        }

        uint32_t length = read_u32();
        (void)length; // total length of the file, can be ignored

        uint32_t jsonChunkLength = read_u32();
        uint32_t jsonChunkType = read_u32();
        if (jsonChunkType != 0x4E4F534A) // "JSON"
        {
            return Status::FAILURE;
        }

        std::vector<uint8_t> jsonChunkData(jsonChunkLength);
        fileStream.read(reinterpret_cast<char *>(jsonChunkData.data()), jsonChunkLength);

        // std::ofstream jsonFile("glTF.json");
        // jsonFile.write(reinterpret_cast<const char *>(jsonChunkData.data()), jsonChunkLength);
        // jsonFile.close();

        nlohmann::json json = nlohmann::json::parse(jsonChunkData.begin(), jsonChunkData.end());
        model = json.get<Model>();

        // Initialize parent indices
        for (Model::NodeIndex i = 0; i < static_cast<Model::NodeIndex>(model.nodes.size()); i++)
        {
            for (Model::NodeIndex childIndex : model.nodes[i].children)
            {
                model.nodes[childIndex].parent = i;
            }
        }

        uint32_t binChunkLength = read_u32();
        uint32_t binChunkType = read_u32();
        if (binChunkType != 0x004E4942) // "BIN"
        {
            return Status::FAILURE;
        }

        std::vector<uint8_t> binChunkData(binChunkLength);
        fileStream.read(reinterpret_cast<char *>(binChunkData.data()), binChunkLength);

        model.data = std::move(binChunkData);

        // Load the GLTF file and populate the model
        return Status::SUCCESS;
    }
}