

#include <iostream>
#include <chrono>
#include <vector>
#include <algorithm>
#include <random>
#include "sim_api.h"

void init()
{
    // Initialization code here
    // This could include setting up data structures, allocating memory, etc.
}
int main(int argc, char **argv)
{

    init(); // Call the initialization function
    std::cout << "Starting microbenchmark..." << std::endl;
    SimRoiStart();
    /*
    @kanellok: ""

    Add your code here to benchmark address translation.
    The SimRoiStart() and SimRoiEnd() functions mark the start and end of your benchmark.
    It would be good if you wrap ONLY the heavy lifting code in these functions so that
    you do not consider initialization/finalization time in your benchmark.

    */
    SimRoiEnd();

    std::cout << "Microbenchmark completed." << std::endl;

    return 0;
}