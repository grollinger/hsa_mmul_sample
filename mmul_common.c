struct args_t {
    void* a;
    void* b;
    void* c;
    uint64_t acols;
} __attribute__ ((aligned(16)));

hsa_buf a;
hsa_buf b;
hsa_buf c;
hsa_buf expected;

/* Internal Parameters */
const size_t ACOLS = 1000;
const size_t AROWS = 500;
const size_t ASIZE = ACOLS * AROWS;
const size_t BSIZE = ACOLS * AROWS;
const size_t CSIZE = AROWS * AROWS;


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

void setup(struct args_t* args) {

    alloc_array(ASIZE, true, &a); randomize_array(a.ptr, ASIZE);
    alloc_array(BSIZE, true, &b); randomize_array(b.ptr, BSIZE);
    alloc_array(CSIZE, true, &c); randomize_array(c.ptr, CSIZE);
    alloc_array(CSIZE, false, &expected);
    julia_mmul(a.ptr,b.ptr,expected.ptr);

    args->a = a.ptr;
    args->b = b.ptr;
    args->c = c.ptr;
    args->acols = ACOLS;
}

void validate(const struct args_t* args) {
    assert_array_equal(expected.ptr, args->c, CSIZE);
}

void cleanup(struct args_t* args) {
    free_buf(&a);
    free_buf(&b);
    free_buf(&c);
    free_buf(&expected);
}
