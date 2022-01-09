#include <stdio.h>

#include <wasmedge/wasmedge.h>

int main(int argc, const char *argv[]) {
  printf("WasmEdge version: %s\n", WasmEdge_VersionGet());
  return 0;
}
