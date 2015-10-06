#include "header.c"

#include "mmul_common.c"

/* Kernel Launch Parameters */
const char MODULE_FILE[] = "mmul2d.brig";
const char KERNEL_NAME[] = "&mmul2d";

const size_t GRID_DIM = 2;
const size_t GRID_X = AROWS;
const size_t GRID_Y = AROWS;
const size_t GRID_Z = 1;

#include "main.c"
