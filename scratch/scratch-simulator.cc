/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/my-queue-disc.h"

#include <random>
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ScratchSimulator");

int func(int a, int b, int c){
    return a + b+ c;
}

int
main(int argc, char* argv[])
{


    NS_LOG_UNCOND("Scratch Simulator");
    auto f = MakeBoundCallback(func, 1);
    std::cout << f(2, 2);
    std::cout << f(3, 2);
    std::cout << f(4, 2);
    std::cout << std::endl;
    uint32_t m = DataRate("10Gbps") * Time("100us") / 8 / 1500;

    std::cout << m << std::endl;
    std::map<uint32_t, uint32_t> counter;
    counter[1]++;
    std::cout << counter[1];
    std::cout << counter[2];
    std::cout << "====\n";
    std::queue<int> queue;
    queue.push(1);
    queue.push(2);
    queue.push(3);
    queue.pop();
    std::cout << queue.front() << " " << queue.front() << std::endl;

    std::random_device rd; // obtain a random number from hardware
    std::mt19937 gen(rd()); // seed the generator
    QueueCounter qc(5);
    std::uniform_int_distribution<uint32_t> distr(0, 5);
    for (uint32_t i = 0; i < 10; ++i)
    {
        uint32_t u = distr(gen);
        qc.Dequeue(u);
        qc.IncrementAndMark(u);
        std::cout << u << "====\n" ;
        for (const auto &[k, v] : qc.GetCounter())
        {
            std::cout << "m[" << k << "] = (" << v << ") " << " \n";

        }
        auto [total, categories,mean, current] = qc.GetState();
        std::cout << total << " " << current << " " << categories << " " << mean;
        std::cout<<std::endl;
        std::cout<<std::endl;
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
