// RUN: %bchk %s -- -I headers
// CHECK: ERROR: Function f lacks a body
// CHECK: ERROR: verification failed

#include "borrowck.h"

int main(int argc, char **argv) {
    return 0;
}
