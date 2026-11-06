// RUN: %bchk %s -- -I headers

#include "borrowck.h"

int main(int argc, char OWNER **argv) {
    _Atomic int a = 0;
    return a;
}