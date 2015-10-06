#include "header.c"

#include "mmul_common.c"

/* Kernel Launch Parameters */
const char MODULE_FILE[] = "mmul.brig";
const char KERNEL_NAME[] = "&mmul";

const size_t GRID_DIM = 1;
const size_t GRID_X = AROWS;
const size_t GRID_Y = 1;
const size_t GRID_Z = 1;

#include "main.c"
