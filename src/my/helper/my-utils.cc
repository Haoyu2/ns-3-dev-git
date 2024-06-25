#include "my-utils.h"

namespace ns3
{


void MyInit()
{
    SystemPath::MakeDirectories(RESULT_DIR);
}

void CreateFolderIfNotExists(const char* path)
{
    // Define the path to the folder
    std::filesystem::path folderPath(path);

    // Check if the folder exists
    if (!std::filesystem::exists(folderPath))
    {
        // Create the folder
        if (std::filesystem::create_directories(folderPath))
        {
            std::cout << "Folder created successfully: " << folderPath << std::endl;
        }
        else
        {
            std::cerr << "Failed to create folder: " << folderPath << std::endl;
        }
    }
    else
    {
        std::cout << "Folder already exists: " << folderPath << std::endl;
    }
}

}
