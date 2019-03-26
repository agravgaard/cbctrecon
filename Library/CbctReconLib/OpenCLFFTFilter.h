#ifndef OPENCLFFTFILTER_H
#define OPENCLFFTFILTER_H

#include <complex>
#include <vector>

#ifdef __APPLE__
#include <OpenCL/cl.h>
#else
#include <CL/cl.h>
#endif

#include "cbctrecon_types.h"

void OpenCL_padding(const cl_int4& paddingIndex, const cl_uint4& paddingSize,
                    const cl_uint4& inputSize, const float *hostVolume,
                    float *hostPaddedVolume,
                    const std::vector<float> &mirrorWeights);

void OpenCL_fft_convolution(cl_int4 inputDimension, cl_int2 kernelDimension,
                            float *hostProjection,
                            std::complex<float> *deviceKernelFFT);

void OpenCL_subtract3Dfrom2DbySlice_InPlace(
    cl_float *buffer, const cl_float *sub_buffer,
    const FloatImageType::SizeType& inputSize,
    const FloatImage2DType::SizeType& subSize);

// Actually is divide ln(65535/X) by ln(65535/Y)
itk::Image<float, 3U>::Pointer OpenCL_divide3Dby3D_OutOfPlace(
    const UShortImageType::Pointer &Num3D,
    const UShortImageType::Pointer &Denum3D);

void OpenCL_AddConst_InPlace(cl_float *buffer,
                             const FloatImageType::SizeType& inputSize,
                             cl_float constant);

void OpenCL_AddConst_MulConst_InPlace(cl_float *buffer,
                                      const FloatImageType::SizeType& inputSize,
                                      cl_float add_constant,
                                      cl_float mul_constant);

void OpenCL_AddConst_InPlace_2D(cl_float *buffer,
                                const FloatImage2DType::SizeType& inputSize,
                                cl_float constant);

cl_float2 OpenCL_min_max(const cl_float *buffer,
                         const FloatImageType::SizeType& inputSize);

cl_float2 OpenCL_min_max_2D(const cl_float *buffer,
                            const FloatImage2DType::SizeType& inputSize);

cl_float2 OpenCL_min_max_recurse(const cl_float2 *buffer, cl_uint inputSize);

#endif // OPENCLFFTFILTER_H
