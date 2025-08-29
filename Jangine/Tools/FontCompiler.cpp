#if defined(JANGINE_FONT_COMPILER)
#include "../Shared.hpp"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <output_header.hpp> [--namespace=<namespace>] <font>\n";
        return 1;
    }

    try
    {
        std::string headerFileName;
        std::vector<std::string> fontFiles;
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
                fontFiles.push_back(arg);
            }
        }

        // Derive array and default namespace names from header file name if not provided
        auto stem = std::filesystem::path(headerFileName).stem().string();
        std::string arrayName = stem + "_data";
        if (namespaceName.empty())
            namespaceName = stem;

        for (const auto &file : fontFiles)
        {
            std::ifstream in(file, std::ios::in | std::ios::binary);
            if (!in)
            {
                std::cerr << "Failed to open shader file: " << file << "\n";
                continue;
            }

            std::ostringstream oss(std::ios::binary);
            oss << in.rdbuf();

            Jangine::IO::CreateBinaryHeader(
                oss.view(),
                headerFileName,
                arrayName,
                namespaceName);
            std::cout << "Header generated: " << headerFileName << "\n";
        }
    }
    catch (const std::exception &ex)
    {
        std::cerr << "Error: " << ex.what() << "\n";
        return 3;
    }

    return 0;
}
#endif