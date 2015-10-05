/* Copyright 2014 HSA Foundation Inc.  All Rights Reserved.
 *
 * HSAF is granting you permission to use this software and documentation (if
 * any) (collectively, the "Materials") pursuant to the terms and conditions
 * of the Software License Agreement included with the Materials.  If you do
 * not have a copy of the Software License Agreement, contact the  HSA Foundation for a copy.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS WITH THE SOFTWARE.
 */

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

const char MODULE_FILE[] = "mmul.brig";
const char KERNEL[] = "&mmul";
const size_t AROWS = 1000;
const size_t ACOLS = 1000;

double* alloc_array() {
    size_t array_bytes = AROWS * ACOLS * sizeof(double);

    double* in=(double*)malloc(array_bytes);
    memset(in, 1, array_bytes);
    hsa_status_t err=hsa_memory_register(in, array_bytes);
    check(Registering argument memory for input parameter, err);

    return in;
}

void randomize_array(double* arr) {
    double low = -1.0;
    double high = 1.0;

    for(size_t i = 0U; i < AROWS * ACOLS; ++i) {
        double r = ((double)rand() * (high - low)) / (double)RAND_MAX + low;
        arr[i] = r;
    }
}

void julia_mmul(double* a, double* b, double* c){
    for(size_t i = 0; i < ACOLS; ++i) {
        for(size_t j = 0; j < AROWS; ++j) {
            double c_ij = 0.0;
            for(size_t k = 0; k < ACOLS; ++k) {
                c_ij += a[k*AROWS + j] * b[i * ACOLS + k];
            }
            c[i*AROWS + j] = c_ij;
        }
    }
}

/*
 * Loads a BRIG module from a specified file. This
 * function does not validate the module.
 */
int load_module_from_file(const char* file_name, hsa_ext_module_t* module) {
    int rc = -1;

    FILE *fp = fopen(file_name, "rb");

    if (!fp) {
        printf("Could not open module file %s \n", file_name);
        exit(-1);
    }

    rc = fseek(fp, 0, SEEK_END);

    size_t file_size = (size_t) (ftell(fp) * sizeof(char));

    rc = fseek(fp, 0, SEEK_SET);

    char* buf = (char*) malloc(file_size);

    memset(buf,0,file_size);

    size_t read_size = fread(buf,sizeof(char),file_size,fp);

    if(read_size != file_size) {
        free(buf);
    } else {
        rc = 0;
        *module = (hsa_ext_module_t) buf;
    }

    fclose(fp);

    return rc;
}

/*
 * Determines if the given agent is of type HSA_DEVICE_TYPE_GPU
 * and sets the value of data to the agent handle if it is.
 */
static hsa_status_t get_gpu_agent(hsa_agent_t agent, void *data) {
    hsa_status_t status;
    hsa_device_type_t device_type;
    status = hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type);
    if (HSA_STATUS_SUCCESS == status && HSA_DEVICE_TYPE_GPU == device_type) {
        hsa_agent_t* ret = (hsa_agent_t*)data;
        *ret = agent;
        return HSA_STATUS_INFO_BREAK;
    }
    return HSA_STATUS_SUCCESS;
}

/*
 * Determines if a memory region can be used for kernarg
 * allocations.
 */
static hsa_status_t get_kernarg_memory_region(hsa_region_t region, void* data) {
    hsa_region_segment_t segment;
    hsa_region_get_info(region, HSA_REGION_INFO_SEGMENT, &segment);
    if (HSA_REGION_SEGMENT_GLOBAL != segment) {
        return HSA_STATUS_SUCCESS;
    }

    hsa_region_global_flag_t flags;
    hsa_region_get_info(region, HSA_REGION_INFO_GLOBAL_FLAGS, &flags);
    if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG) {
        hsa_region_t* ret = (hsa_region_t*) data;
        *ret = region;
        return HSA_STATUS_INFO_BREAK;
    }

    return HSA_STATUS_SUCCESS;
}

int main(int argc, char **argv) {
    srand(time(NULL));

    hsa_status_t err;

    err = hsa_init();
    check(Initializing the hsa runtime, err);

    /*
     * Iterate over the agents and pick the gpu agent using
     * the get_gpu_agent callback.
     */
    hsa_agent_t agent;
    err = hsa_iterate_agents(get_gpu_agent, &agent);
    if(err == HSA_STATUS_INFO_BREAK) { err = HSA_STATUS_SUCCESS; }
    check(Getting a gpu agent, err);

    /*
     * Query the name of the agent.
     */
    char name[64] = { 0 };
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name);
    check(Querying the agent name, err);
    printf("The agent name is %s.\n", name);

    /*
     * Query the maximum size of the queue.
     */
    uint32_t queue_size = 0;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_QUEUE_MAX_SIZE, &queue_size);
    check(Querying the agent maximum queue size, err);
    printf("The maximum queue size is %u.\n", (unsigned int) queue_size);

    /*
     * Create a queue using the maximum size.
     */
    hsa_queue_t* queue;
    err = hsa_queue_create(agent, queue_size, HSA_QUEUE_TYPE_SINGLE, NULL, NULL, UINT32_MAX, UINT32_MAX, &queue);
    check(Creating the queue, err);

    /*
     * Load the BRIG binary.
     */
    hsa_ext_module_t module;
    load_module_from_file(MODULE_FILE,&module);

    /*
     * Create hsa program.
     */
    hsa_ext_program_t program;
    memset(&program,0,sizeof(hsa_ext_program_t));
    err = hsa_ext_program_create(HSA_MACHINE_MODEL_LARGE, HSA_PROFILE_FULL, HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT, NULL, &program);
    check(Create the program, err);

    /*
     * Add the BRIG module to hsa program.
     */
    err = hsa_ext_program_add_module(program, module);
    check(Adding the brig module to the program, err);

    /*
     * Determine the agents ISA.
     */
    hsa_isa_t isa;
    err = hsa_agent_get_info(agent, HSA_AGENT_INFO_ISA, &isa);
    check(Query the agents isa, err);

    /*
     * Finalize the program and extract the code object.
     */
    hsa_ext_control_directives_t control_directives;
    memset(&control_directives, 0, sizeof(hsa_ext_control_directives_t));
    hsa_code_object_t code_object;
    err = hsa_ext_program_finalize(program, isa, 0, control_directives, "", HSA_CODE_OBJECT_TYPE_PROGRAM, &code_object);
    check(Finalizing the program, err);

    /*
     * Destroy the program, it is no longer needed.
     */
    err=hsa_ext_program_destroy(program);
    check(Destroying the program, err);

    /*
     * Create the empty executable.
     */
    hsa_executable_t executable;
    err = hsa_executable_create(HSA_PROFILE_FULL, HSA_EXECUTABLE_STATE_UNFROZEN, "", &executable);
    check(Create the executable, err);

    /*
     * Load the code object.
     */
    err = hsa_executable_load_code_object(executable, agent, code_object, "");
    check(Loading the code object, err);

    /*
     * Freeze the executable; it can now be queried for symbols.
     */
    err = hsa_executable_freeze(executable, "");
    check(Freeze the executable, err);

   /*
    * Extract the symbol from the executable.
    */
    hsa_executable_symbol_t symbol;
    err = hsa_executable_get_symbol(executable, NULL, KERNEL, agent, 0, &symbol);
    check(Extract the symbol from the executable, err);

    /*
     * Extract dispatch information from the symbol
     */
    uint64_t kernel_object;
    uint32_t kernarg_segment_size;
    uint32_t group_segment_size;
    uint32_t private_segment_size;
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT, &kernel_object);
    check(Extracting the symbol from the executable, err);
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_KERNARG_SEGMENT_SIZE, &kernarg_segment_size);
    check(Extracting the kernarg segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE, &group_segment_size);
    check(Extracting the group segment size from the executable, err);
    err = hsa_executable_symbol_get_info(symbol, HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE, &private_segment_size);
    check(Extracting the private segment from the executable, err);

    /*
     * Create a signal to wait for the dispatch to finish.
     */
    hsa_signal_t signal;
    err=hsa_signal_create(1, 0, NULL, &signal);
    check(Creating a HSA signal, err);

    /*
     * Allocate and initialize the kernel arguments and data.
     */

    double* a=alloc_array(); randomize_array(a);
    double* b=alloc_array(); randomize_array(b);
    double* c=alloc_array(); randomize_array(c);
    double* c_expected=alloc_array();
    julia_mmul(a,b,c_expected);

    struct __attribute__ ((aligned(16))) args_t {
        void* a;
        void* b;
        void* c;
        uint64_t arows;
        uint64_t acols;
    } args;

    args.a=a;
    args.b=b;
    args.c=c;
    args.arows = AROWS;
    args.acols = ACOLS;

    /*
     * Find a memory region that supports kernel arguments.
     */
    hsa_region_t kernarg_region;
    kernarg_region.handle=(uint64_t)-1;
    hsa_agent_iterate_regions(agent, get_kernarg_memory_region, &kernarg_region);
    err = (kernarg_region.handle == (uint64_t)-1) ? HSA_STATUS_ERROR : HSA_STATUS_SUCCESS;
    check(Finding a kernarg memory region, err);
    void* kernarg_address = NULL;

    /*
     * Allocate the kernel argument buffer from the correct region.
     */
    err = hsa_memory_allocate(kernarg_region, kernarg_segment_size, &kernarg_address);
    check(Allocating kernel argument memory buffer, err);
    memcpy(kernarg_address, &args, sizeof(args));

    /*
     * Obtain the current queue write index.
     */
    uint64_t index = hsa_queue_load_write_index_relaxed(queue);

    /*
     * Write the aql packet at the calculated queue index address.
     */
    const uint32_t queueMask = queue->size - 1;
    hsa_kernel_dispatch_packet_t* dispatch_packet = &(((hsa_kernel_dispatch_packet_t*)(queue->base_address))[index&queueMask]);

    dispatch_packet->setup  |= 1 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    dispatch_packet->workgroup_size_x = (uint16_t)1;
    dispatch_packet->workgroup_size_y = (uint16_t)1;
    dispatch_packet->workgroup_size_z = (uint16_t)1;
    dispatch_packet->grid_size_x = (uint32_t) (AROWS);
    dispatch_packet->grid_size_y = 1;
    dispatch_packet->grid_size_z = 1;
    dispatch_packet->completion_signal = signal;
    dispatch_packet->kernel_object = kernel_object;
    dispatch_packet->kernarg_address = (void*) kernarg_address;
    dispatch_packet->private_segment_size = private_segment_size;
    dispatch_packet->group_segment_size = group_segment_size;

    uint16_t header = 0;
    header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
    header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE;
    header |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;

    printf("DispatchPacket prepared, launching kernel\n");

    struct timespec start, stop, elapsed;
    clock_gettime(CLOCK_MONOTONIC, &start);

    __atomic_store_n((uint16_t*)(&dispatch_packet->header), header, __ATOMIC_RELEASE);

    /*
     * Increment the write index and ring the doorbell to dispatch the kernel.
     */
    hsa_queue_store_write_index_relaxed(queue, index+1);
    hsa_signal_store_relaxed(queue->doorbell_signal, index);

    /*
     * Wait on the dispatch completion signal until the kernel is finished.
     */
    hsa_signal_value_t value = hsa_signal_wait_acquire(signal, HSA_SIGNAL_CONDITION_LT, 1, UINT64_MAX, HSA_WAIT_STATE_BLOCKED);

    clock_gettime(CLOCK_MONOTONIC, &stop);

    check(Dispatching the kernel, err);

    elapsed = diff(start, stop);
    printf("Kernel took %li s %li ns\n", elapsed.tv_sec, elapsed.tv_nsec);


    /*
     * Validate the data in the output buffer.
     */
    int valid=1;
    int fail_index=0;
    for(int i=0; i<AROWS*AROWS; i++) {
        if(c[i]!=c_expected[i]) {
            fail_index=i;
            valid=0;
            break;
        }
    }

    if(valid) {
        printf("Passed validation.\n");
    } else {
        printf("VALIDATION FAILED!\nBad index: %d (%f != %f)\n", fail_index, c[fail_index], c_expected[fail_index]);
    }

    /*
     * Cleanup all allocated resources.
     */
    err = hsa_memory_free(kernarg_address);
    check(Freeing kernel argument memory buffer, err);

    err=hsa_signal_destroy(signal);
    check(Destroying the signal, err);

    err=hsa_executable_destroy(executable);
    check(Destroying the executable, err);

    err=hsa_code_object_destroy(code_object);
    check(Destroying the code object, err);

    err=hsa_queue_destroy(queue);
    check(Destroying the queue, err);

    err=hsa_shut_down();
    check(Shutting down the runtime, err);

    free(a);
    free(b);
    free(c);
    free(c_expected);

    return 0;
}
