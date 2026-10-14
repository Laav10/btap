#include <cstdlib>
#include <iostream>

int main(int argc, char **argv) {
  const int index = argc == 2 ? std::atoi(argv[1]) : 7;
  int values[8] = {};
  values[index] = 42;
  std::cout << values[index] << '\n';
}
