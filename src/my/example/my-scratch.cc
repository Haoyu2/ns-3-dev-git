#include <iostream>
#include <string>
#include "ns3/core-module.h"

using namespace ns3;
int
main(int argc, char* argv[])
{
    std::cout << "FindSelfDirectory:   " << SystemPath::FindSelfDirectory() << std::endl;
//    std::cout << "NS-3 Root Directory: " << rootDir << std::endl;
    return 0;
}
