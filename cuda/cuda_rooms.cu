#include <cuda_runtime.h>
#include "../c/common.h"

#define CUDA_CHECK(call) do { cudaError_t e_ = (call); if (e_ != cudaSuccess) { \
    fprintf(stderr, "CUDA error %s at %s:%d\n", cudaGetErrorString(e_), __FILE__, __LINE__); \
    exit(1); } } while (0)

__global__ void analyze_kernel(const int *temps, int rooms, int cols,
                               int *hot_out, int *run_out, int *cat_out)
{
    int r = blockIdx.x * blockDim.x + threadIdx.x;
    if (r >= rooms)
        return;

    const int *row = temps + (size_t)r * cols;
    int hot = 0, run = 0, best = 0;
    for (int i = 0; i < cols; i++) {
        if (row[i] > HOT_LIMIT) {
            hot++;
            if (++run > best) best = run;
        } else {
            run = 0;
        }
    }
    hot_out[r] = hot;
    run_out[r] = best;
    cat_out[r] = (hot == 0) ? NORMAL : (best >= 3 ? URGENT : WATCH);
}

int main(int argc, char **argv)
{
    char filename[1024];
    int B;
    if (argc > 2) {
        snprintf(filename, sizeof filename, "%s", argv[1]);
        B = atoi(argv[2]);
    } else {
        ask_filename(filename);
        printf("Threads per block: ");
        fflush(stdout);
        if (scanf("%d", &B) != 1) return 1;
    }

    Dataset ds;
    if (load_csv(filename, &ds) != 0)
        return 1;
    int R = ds.rooms, C = ds.cols;

    cudaDeviceProp prop;
    CUDA_CHECK(cudaGetDeviceProperties(&prop, 0));
    if (B < 1 || B > prop.maxThreadsPerBlock) {
        int repl = prop.maxThreadsPerBlock < 256 ? prop.maxThreadsPerBlock : 256;
        printf("Block size %d unsupported (device limit %d); using %d\n",
               B, prop.maxThreadsPerBlock, repl);
        B = repl;
    }
    int blocks = (R + B - 1) / B;
    printf("GPU: %s | R = %d, C = %d, B = %d, blocks = %d\n", prop.name, R, C, B, blocks);

    size_t tbytes = (size_t)R * C * sizeof(int), rbytes = (size_t)R * sizeof(int);
    int *d_temps, *d_hot, *d_run, *d_cat;
    CUDA_CHECK(cudaMalloc(&d_temps, tbytes));
    CUDA_CHECK(cudaMalloc(&d_hot, rbytes));
    CUDA_CHECK(cudaMalloc(&d_run, rbytes));
    CUDA_CHECK(cudaMalloc(&d_cat, rbytes));

    int *h_hot = (int *)malloc(rbytes), *h_run = (int *)malloc(rbytes), *h_cat = (int *)malloc(rbytes);
    RoomResult *res = (RoomResult *)malloc(R * sizeof(RoomResult));
    if (!h_hot || !h_run || !h_cat || !res) { perror("malloc"); return 1; }

    cudaEvent_t e0, e1;
    CUDA_CHECK(cudaEventCreate(&e0));
    CUDA_CHECK(cudaEventCreate(&e1));

    Totals tot;
    double times[NUM_RUNS], ktimes[NUM_RUNS];
    for (int run = 0; run < NUM_RUNS; run++) {
        CUDA_CHECK(cudaDeviceSynchronize());
        double start = now_seconds();

        CUDA_CHECK(cudaMemset(d_hot, 0, rbytes));
        CUDA_CHECK(cudaMemset(d_run, 0, rbytes));
        CUDA_CHECK(cudaMemset(d_cat, 0, rbytes));
        CUDA_CHECK(cudaMemcpy(d_temps, ds.temps, tbytes, cudaMemcpyHostToDevice));

        CUDA_CHECK(cudaEventRecord(e0));
        analyze_kernel<<<blocks, B>>>(d_temps, R, C, d_hot, d_run, d_cat);
        CUDA_CHECK(cudaGetLastError());
        CUDA_CHECK(cudaEventRecord(e1));
        CUDA_CHECK(cudaDeviceSynchronize());

        CUDA_CHECK(cudaMemcpy(h_hot, d_hot, rbytes, cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_run, d_run, rbytes, cudaMemcpyDeviceToHost));
        CUDA_CHECK(cudaMemcpy(h_cat, d_cat, rbytes, cudaMemcpyDeviceToHost));
        for (int r = 0; r < R; r++) {
            res[r].hot_count = h_hot[r];
            res[r].longest_run = h_run[r];
            res[r].category = h_cat[r];
        }
        compute_totals(&tot, res, R);
        times[run] = now_seconds() - start;

        float ms;
        CUDA_CHECK(cudaEventElapsedTime(&ms, e0, e1));
        ktimes[run] = ms / 1000.0;
    }

    print_small_table(&ds, res);
    printf("\n");
    print_totals(&tot, R);
    write_results("results_cuda.csv", &ds, res);
    report_times("cuda", filename, B, times);
    printf("Kernel-only times (s):");
    for (int i = 1; i < NUM_RUNS; i++) printf(" %.6f", ktimes[i]);
    printf("\n");
    printf("BLOCKS,%d,%d,%d\n", R, B, blocks);

    CUDA_CHECK(cudaFree(d_temps)); CUDA_CHECK(cudaFree(d_hot));
    CUDA_CHECK(cudaFree(d_run));   CUDA_CHECK(cudaFree(d_cat));
    free(h_hot); free(h_run); free(h_cat); free(res); free(ds.ids); free(ds.temps);
    return 0;
}
