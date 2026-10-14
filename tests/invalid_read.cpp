#include <cstdlib>

int main(int argc, char **argv) {
  int values[8] = {};
  return values[std::atoi(argv[1])];
}
