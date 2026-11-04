// RUN: %bchk %s -- -I headers
// CHECK: ERROR: Function f lacks a body
// CHECK: ERROR: verification failed

#include "borrowck.h"

int main(int argc, char OWNER **argv) {
    return 0;
}

int f(void);
