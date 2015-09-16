#pragma OPENCL EXTENSION cl_khr_fp64 : enable

kernel void mmul_kernel(
                        global double* a,
                        global double* b,
                        global double* c,
                        ulong arows,
                        ulong acols) {
  uint i = get_global_id(0);
  for(uint j = 0; j < arows; ++j) {
    double c_ij = 0.0;
    for(uint k = 0; k < acols; ++k) {
      c_ij += a[k*arows + j] * b[i * acols + k];
    }
    c[i*arows + j] = c_ij;
  }
}


// vim: ts=2 et sw=2
