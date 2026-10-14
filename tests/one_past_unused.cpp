// Forming a one-past pointer is legal when it is not dereferenced. BTAP must
// instrument the load/store, not this GEP expression.
int main() {
  int values[8] = {};
  int *end = values + 8;
  (void)end;
  values[0] = 42;
  return values[0] == 42 ? 0 : 1;
}
