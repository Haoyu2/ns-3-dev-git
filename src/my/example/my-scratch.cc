#include <iostream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/my-utils.h"
// defined in line 19 marcos-and-definitions
//add_definitions(-DPROJECT_SOURCE_PATH="${PROJECT_SOURCE_DIR}")
using namespace ns3;
int
main(int argc, char* argv[])
{
    MyInit();
     Time recordingInterval = Seconds(0.5);     //
     Time recordQueueSizeInterval = MilliSeconds(21);
        auto n = (recordingInterval / recordQueueSizeInterval);
     std::cout << n.GetInt();
//    std::cout << "FindSelfDirectory:   " << SystemPath::FindSelfDirectory() << std::endl;
//    std::cout << "FindSelfDirectory:   " << PROJECT_SOURCE_PATH << std::endl;
//    std::cout << "NS-3 Root Directory: " << rootDir << std::endl;
    return 0;
}
