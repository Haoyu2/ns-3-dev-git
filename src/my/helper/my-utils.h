#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <string>
#include "ns3/core-module.h"
#include <filesystem>

namespace ns3
{
inline std::string RESULT_DIR = std::string (PROJECT_SOURCE_PATH) + "/A";
void MyInit();
void CreateFolderIfNotExists(const char* path);
}


#endif
