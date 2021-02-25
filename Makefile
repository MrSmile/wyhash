
#CXX = clang++ -g -Ofast -march=native
CXX = g++ -g -Ofast -march=native
#CXX = g++ -g -O0

.PHONY: all
all: wyhash

test_vector: test_vector.cpp wyhash.h Makefile
	$(CXX) test_vector.cpp -o test_vector

wyhash: benchmark.cpp wyhash.h Makefile
	$(CXX) benchmark.cpp -o wyhash

.PHONY: clean
clean:
	rm test_vector wyhash

