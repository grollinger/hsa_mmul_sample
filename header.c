// enable POSIX 2008
#define _XOPEN_SOURCE 700

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "hsa.h"
#include "hsa_ext_finalize.h"

#define check(msg, status) \
if (status != HSA_STATUS_SUCCESS) { \
    printf("%s failed.\n", #msg); \
    exit(1); \
} else { \
   printf("%s succeeded.\n", #msg); \
}

struct timespec diff(struct timespec start, struct timespec end) {
  struct timespec temp;
  if ((end.tv_nsec-start.tv_nsec)<0) {
    temp.tv_sec = end.tv_sec-start.tv_sec-1;
    temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
  } else {
    temp.tv_sec = end.tv_sec-start.tv_sec;
    temp.tv_nsec = end.tv_nsec-start.tv_nsec;
  }
  return temp;
}

typedef struct {
    void* ptr;
    size_t size;
    int is_registered;
} hsa_buf;

void alloc_array(size_t len, int hsa_register, hsa_buf* out) {
    size_t array_bytes = len * sizeof(double);

    double* in=(double*)malloc(array_bytes);
    memset(in, 1, array_bytes);
    if (hsa_register) {
        hsa_status_t err=hsa_memory_register(in, array_bytes);
        check(Registering argument memory, err);
    }

    out->ptr = in;
    out->size = array_bytes;
    out->is_registered = hsa_register;
}

void free_buf(hsa_buf* buf) {
    if(!buf || !buf->ptr) {
        return;
    }

    if(buf->is_registered) {
        hsa_memory_deregister(buf->ptr, buf->size);
    }

    free(buf->ptr);
    buf->ptr = NULL;
    buf->size = 0;
}

void randomize_array(double* arr, size_t len) {
    double low = -1.0;
    double high = 1.0;

    for(size_t i = 0U; i < len; ++i) {
        double r = ((double)rand() * (high - low)) / (double)RAND_MAX + low;
        arr[i] = r;
    }
}

void assert_array_equal(const double* expected, const double* value, size_t len) {
    /*
     * Validate the data in the output buffer.
     */
    int valid=1;
    int fail_index=0;
    for(int i=0; i<len; i++) {
        if(value[i]!=expected[i]) {
            fail_index=i;
            valid=0;
            break;
        }
    }

    if(valid) {
        printf("Passed validation. \n");
    } else {
        printf("VALIDATION FAILED!\nBad index: %d (value: %f != expected: %f)\n",
            fail_index, value[fail_index], expected[fail_index]);
    }
}

