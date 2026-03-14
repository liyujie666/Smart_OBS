#include <stdio.h>

__global__ void hello() {
    printf("Thread %d: Hello from RTX 2060!\n", threadIdx.x);
}

int main() {
    hello<<<1, 5>>>();
    cudaDeviceSynchronize();
    return 0;
}