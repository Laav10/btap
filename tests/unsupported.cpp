// Deliberately unsupported: a heap allocation is not claimed as protected.
#include <cstdlib>

int main(int argc, char **argv) {
  int *values = new int[8]{};
  values[std::atoi(argv[1])] = 1;
  delete[] values;
}
