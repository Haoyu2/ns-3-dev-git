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
     Time recordingInterval = Seconds(1);     //
     Time recordQueueSizeInterval = MilliSeconds(1);
        auto n = (recordingInterval / recordQueueSizeInterval);
     std::cout << n.GetInt();

     Time time1 = Seconds(0);
     for (int i = 0; i < 5; ++i)
     {
         std::cout << std::setfill(' ') << std::setw(7)
                   << std::fixed << std::showpoint
                << std::setprecision(1)
                <<  time1.GetSeconds() << "s ";
         time1 += Seconds(0.5);
     }

     std::cout << "\n";
     for (int i = 0; i < 20; ++i)
     {
         std::cout << std::setfill('0') << std::setw(2) << i << "\n";
     }


     for (int i = 0; i < 20; ++i)
     {
         std::cout << std::setfill('0') << std::setw(2) << i << " ";
         for (int j = 0; j < 5; ++j)
         {
             std::cout << std::setfill(' ') << std::setw(7)
                    << std::fixed << std::showpoint
                    << std::setprecision(1)
                    << j * 52.0/3.0 << " ";
         }
         std::cout << "\n";
     }
     std::cout << "\n";


     //    std::cout << "FindSelfDirectory:   " << SystemPath::FindSelfDirectory() << std::endl;
//    std::cout << "FindSelfDirectory:   " << PROJECT_SOURCE_PATH << std::endl;
//    std::cout << "NS-3 Root Directory: " << rootDir << std::endl;
    return 0;
}
