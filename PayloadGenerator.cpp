#include "PayloadGenerator.h"


// function to generate an array of random numbers
std::vector<int> PayloadGenerator::generate(size_t size, int min, int max)
{
    // creating a vector of the given size
    std::vector<int> data(size);
    // random number device
    std::random_device rd;
    // random number generator
    std::mt19937 gen(rd());
    // uniform distribution of random numbers
    std::uniform_int_distribution<> dis(min, max);

    // replacement suggested by static analyzer cppcheck
    // filling the vector with random numbers
    std::generate(data.begin(), data.end(), [&]() { return dis(gen); });


    return data;
}