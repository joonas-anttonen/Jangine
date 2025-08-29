#if defined(JANGINE_SHADER_COMPILER)
#include "../Gfx/ShaderProgram.hpp"

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <output_header.hpp> [--namespace=<namespace>] <shader1.hlsl> [shader2.hlsl ...]\n";
        return 1;
    }

    std::string headerFileName;
    std::vector<std::string> shaderFiles;
    std::string namespaceName;

    // Parse arguments
    headerFileName = argv[1];
    for (int i = 2; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg.rfind("--namespace=", 0) == 0)
        {
            namespaceName = arg.substr(12);
        }
        else
        {
            shaderFiles.push_back(arg);
        }
    }

    // Derive array and default namespace names from header file name if not provided
    auto stem = std::filesystem::path(headerFileName).stem().string();
    std::string arrayName = stem + "_data";
    if (namespaceName.empty())
        namespaceName = stem;

    Jangine::Gfx::ShaderCompiler compiler;
    std::unordered_map<std::string, Jangine::Gfx::ShaderProgram> shaderPrograms;

    for (const auto &file : shaderFiles)
    {
        std::ifstream in(file, std::ios::in | std::ios::binary);
        if (!in)
        {
            std::cerr << "Failed to open shader file: " << file << "\n";
            continue;
        }
        std::string source((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        std::string programName = std::filesystem::path(file).stem().string();
        auto program = compiler.Compile(source, programName);
        if (!program)
        {
            std::cout << "Failed to compile shader: " << file << "\n";
            return 2;
        }

        shaderPrograms[programName] = std::move(program.value());
    }

    if (shaderPrograms.empty())
    {
        std::cerr << "No valid shaders compiled. Aborting.\n";
        return 2;
    }

    try
    {
        std::ostringstream oss(std::ios::binary);
        Jangine::Gfx::IO::ShaderPackage::Serialize(shaderPrograms, oss);

        Jangine::IO::CreateBinaryHeader(
            oss.view(),
            headerFileName,
            arrayName,
            namespaceName);
        std::cout << "Header generated: " << headerFileName << "\n";
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 3;
    }

    return 0;
}
#endif