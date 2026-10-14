#include <iostream>

int main(int argc, char **) {
  int values[8] = {};
  values[0] = 10;
  values[7] = 32;
  int sum = 0;
  for (int index = 0; index < 8; ++index)
    sum += values[index];
  // The pass must not report merely because an invalid dereference exists in a
  // branch. With normal invocation this branch is not executed.
  if (argc == 99)
    values[8] = 1; // A non-executed dereference must not report.
  std::cout << sum << '\n';
  return sum == 42 ? 0 : 1;
}
