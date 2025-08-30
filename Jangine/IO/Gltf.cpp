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
        model.ConnectHierarchy();

        uint32_t binChunkLength = read_u32();
        uint32_t binChunkType = read_u32();
        if (binChunkType != 0x004E4942) // "BIN"
        {
            return Status::FAILURE;
        }

        std::vector<uint8_t> binChunkData(binChunkLength);
        fileStream.read(reinterpret_cast<char *>(binChunkData.data()), binChunkLength);

        model.data = std::move(binChunkData);

        return Status::SUCCESS;
    }

    Status SaveToFile(const std::string &filename, const Model &model)
    {
        std::ofstream fileStream(filename, std::ios::binary);
        if (!fileStream)
        {
            return Status::FAILURE;
        }

        nlohmann::json json = model;

        std::string jsonString = json.dump();
        uint32_t jsonPadding = (4 - (jsonString.size() % 4)) % 4;
        uint32_t jsonChunkLength = static_cast<uint32_t>(jsonString.size() + jsonPadding);
        uint32_t jsonChunkType = 0x4E4F534A; // "JSON"

        uint32_t binPadding = (4 - (model.data.size() % 4)) % 4;
        uint32_t binChunkLength = static_cast<uint32_t>(model.data.size() + binPadding);
        uint32_t binChunkType = 0x004E4942; // "BIN"

        uint32_t magic = 0x46546C67; // "glTF"
        uint32_t version = 2;
        uint32_t length = 12 + 8 + static_cast<uint32_t>(jsonChunkLength) + 8 + static_cast<uint32_t>(binChunkLength);

        fileStream.write(reinterpret_cast<const char *>(&magic), sizeof(magic));
        fileStream.write(reinterpret_cast<const char *>(&version), sizeof(version));
        fileStream.write(reinterpret_cast<const char *>(&length), sizeof(length));

        fileStream.write(reinterpret_cast<const char *>(&jsonChunkLength), sizeof(jsonChunkLength));
        fileStream.write(reinterpret_cast<const char *>(&jsonChunkType), sizeof(jsonChunkType));
        fileStream.write(jsonString.data(), jsonString.size());

        // Write padding bytes for JSON chunk
        uint8_t space = 0x20;
        for (uint32_t i = 0; i < jsonPadding; ++i)
        {
            fileStream.write(reinterpret_cast<const char *>(&space), sizeof(space));
        }

        fileStream.write(reinterpret_cast<const char *>(&binChunkLength), sizeof(binChunkLength));
        fileStream.write(reinterpret_cast<const char *>(&binChunkType), sizeof(binChunkType));
        fileStream.write(reinterpret_cast<const char *>(model.data.data()), model.data.size());

        // Write padding bytes for BIN chunk
        uint32_t zero = 0;
        for (uint32_t i = 0; i < binPadding; ++i)
        {
            fileStream.write(reinterpret_cast<const char *>(&zero), sizeof(zero));
        }

        fileStream.close();

        //std::ofstream jsonFile("glTF.json");
        //jsonFile.write(jsonString.c_str(), jsonString.size());
        //jsonFile.close();

        return Status::SUCCESS;
    }
}