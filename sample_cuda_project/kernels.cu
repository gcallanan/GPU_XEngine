// Kernel function to add the elements of two arrays
__global__ void add_kernel(int n, float *x, float *y)
{
  for (int i = 0; i < n; i++)
    y[i] = x[i] + y[i];
}


void add(int N, float *x, float *y){
    add_kernel<<<1, 1>>>(N, x, y);
}