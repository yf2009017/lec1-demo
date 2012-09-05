#include "timing.h"
#include "cl-helper.h"




int main(int argc, char **argv)
{
  if (argc != 3)
  {
    fprintf(stderr, "need two arguments!\n");
    abort();
  }

  const long n = atol(argv[1]);
  const int ntrips = atoi(argv[2]);

  print_platforms_devices();

  cl_context ctx;
  cl_command_queue queue;
  create_context_on("Intel", NULL, 0, &ctx, &queue, 0);

  // --------------------------------------------------------------------------
  // load kernels 
  // --------------------------------------------------------------------------
  char *knl_text = read_file("vec-add.cl");
  cl_kernel knl = kernel_from_string(ctx, knl_text, "sum", NULL);
  free(knl_text);

  // --------------------------------------------------------------------------
  // allocate and initialize CPU memory
  // --------------------------------------------------------------------------
  double *a = (double *) malloc(sizeof(double) * n);
  if (!a) { perror("alloc x"); abort(); }
  double *b = (double *) malloc(sizeof(double) * n);
  if (!b) { perror("alloc y"); abort(); }
  double *c = (double *) malloc(sizeof(double) * n);
  if (!c) { perror("alloc z"); abort(); }

  for (size_t i = 0; i < n; ++i)
  {
    a[i] = i;
    b[i] = 2*i;
  }

  // --------------------------------------------------------------------------
  // allocate device memory
  // --------------------------------------------------------------------------
  cl_int status;
  cl_mem buf_a = clCreateBuffer(ctx, CL_MEM_READ_WRITE, 
      sizeof(double) * n, 0, &status);
  CHECK_CL_ERROR(status, "clCreateBuffer");

  cl_mem buf_b = clCreateBuffer(ctx, CL_MEM_READ_WRITE,
      sizeof(double) * n, 0, &status);
  CHECK_CL_ERROR(status, "clCreateBuffer");

  cl_mem buf_c = clCreateBuffer(ctx, CL_MEM_READ_WRITE,
      sizeof(double) * n, 0, &status);
  CHECK_CL_ERROR(status, "clCreateBuffer");

  // --------------------------------------------------------------------------
  // transfer to device
  // --------------------------------------------------------------------------
  CALL_CL_GUARDED(clEnqueueWriteBuffer, (
        queue, buf_a, /*blocking*/ CL_TRUE, /*offset*/ 0,
        n * sizeof(double), a,
        0, NULL, NULL));

  CALL_CL_GUARDED(clEnqueueWriteBuffer, (
        queue, buf_b, /*blocking*/ CL_TRUE, /*offset*/ 0,
        n * sizeof(double), b,
        0, NULL, NULL));

  // --------------------------------------------------------------------------
  // run code on device
  // --------------------------------------------------------------------------

  struct timespec time1, time2;
  clock_gettime(CLOCK_REALTIME, &time1);

  for (int trip = 0; trip < ntrips; ++trip)
  {
    SET_4_KERNEL_ARGS(knl, buf_a, buf_b, buf_c, n);
    size_t ldim[] = { 1 };
    size_t gdim[] = { 1 };
    CALL_CL_GUARDED(clEnqueueNDRangeKernel,
        (queue, knl,
         /*dimensions*/ 1, NULL, gdim, ldim,
         0, NULL, NULL));
  }

  clock_gettime(CLOCK_REALTIME, &time2);
  double elapsed = timespec_diff_in_seconds(time1,time2)/ntrips;
  printf("%f s\n", elapsed);
  printf("%f GB/s\n",
      3*n*sizeof(double)/1e9/elapsed);

  // --------------------------------------------------------------------------
  // clean up
  // --------------------------------------------------------------------------
  CALL_CL_GUARDED(clFinish, (queue));
  CALL_CL_GUARDED(clReleaseMemObject, (buf_a));
  CALL_CL_GUARDED(clReleaseMemObject, (buf_b));
  CALL_CL_GUARDED(clReleaseMemObject, (buf_c));
  CALL_CL_GUARDED(clReleaseKernel, (knl));
  CALL_CL_GUARDED(clReleaseCommandQueue, (queue));
  CALL_CL_GUARDED(clReleaseContext, (ctx));

  return 0;
}