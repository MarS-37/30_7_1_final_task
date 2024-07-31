#ifndef PAYLOADGENERATOR_H
#define PAYLOADGENERATOR_H


#include <vector>
#include <random>


// generation of a random number array
class PayloadGenerator
{
public:
    // function to generate an array of random numbers
    static std::vector<int> generate(size_t size, int min, int max);
};


#endif // PAYLOADGENERATOR_H