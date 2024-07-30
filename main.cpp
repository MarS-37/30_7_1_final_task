#include <vector>
#include <random>


// generation of a random number array
class PayloadGenerator
{
public:
	// function to generate an array of random numbers
	static std::vector<int> generate(size_t size, int min, int max)
	{
		// creating a vector of the given size
		std::vector<int> vec_number(size);
		// random number device
		std::random_device rd;
		// random number generator
		std::mt19937 gen(rd());
		// uniform distribution of random numbers
		std::uniform_int_distribution<int> dis(min, max);

		// initial solution
		/*for (auto& d : data) {
			d = dis(gen);
		}*/
		// replacement suggested by static analyzer cppcheck
		std::generate(vec_number.begin(), vec_number.end(), [&]() { return dis(gen); });


		// returning the generated vector
		return vec_number;
	}
};