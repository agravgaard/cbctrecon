// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++, C#, and Java:
// http://www.viva64.com

#include "OpenCL/ImageFilters.hpp"
#include "OpenCL/cl2.hpp"
#include "OpenCL/device_picker.hpp"
#include "OpenCL/err_code.hpp"

int main(int argc, char **argv) {

  // Get list of platforms
  std::vector<cl::Platform> platforms;
  cl::Platform::get(&platforms);

  auto i = 0U;
  for (auto &plat : platforms) {
    // Print some platform info:
    std::string plat_name;
    const auto cl_err = plat.getInfo(CL_PLATFORM_NAME, &plat_name);
    checkError(cl_err, "platform name");
    std::cerr << "Platform #" << ++i << ": " << plat_name << "\n";
  }

  std::cerr << "\n";

  auto devices = crl::opencl::getDeviceList();

  i = 0U;
  for (auto &dev : devices) {
    auto cl_err = CL_SUCCESS;
    // Print some device info:
    auto device_name = dev.getInfo<CL_DEVICE_NAME>(&cl_err);
    checkError(cl_err, "device name");
    std::cerr << "Device #" << ++i << ": " << device_name << "\n";

    // OpenCL version?
    auto device_opencl_version = dev.getInfo<CL_DEVICE_VERSION>(&cl_err);
    checkError(cl_err, "device version");
    std::cerr << "Device version: " << device_opencl_version << "\n";

    // Image support?
    auto device_image_support = dev.getInfo<CL_DEVICE_IMAGE_SUPPORT>(&cl_err);
    checkError(cl_err, "device image support");
    if (device_image_support) {
      std::cerr << "Device has image support :)\n";
    } else {
      std::cerr << "Device does NOT support opencl image types! :(\n";
    }

    // Memory?:
    auto max_mem_alloc = dev.getInfo<CL_DEVICE_MAX_MEM_ALLOC_SIZE>(&cl_err);
    checkError(cl_err, "device mem alloc max");
    std::cerr << "Max memory alloc: " << max_mem_alloc / (1024 * 1024)
              << " MB\n";

    auto global_mem = dev.getInfo<CL_DEVICE_GLOBAL_MEM_SIZE>(&cl_err);
    checkError(cl_err, "device global mem");
    std::cerr << "Global memory: " << global_mem / (1024 * 1024) << " MB\n";

    auto local_mem = dev.getInfo<CL_DEVICE_LOCAL_MEM_SIZE>(&cl_err);
    checkError(cl_err, "device local mem");
    std::cerr << "Local memory: " << local_mem / 1024 << " KB\n";

    auto constant_mem =
        dev.getInfo<CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE>(&cl_err);
    checkError(cl_err, "device constant mem");
    std::cerr << "Constant memory: " << constant_mem / 1024 << " KB\n";

    auto compute_units = dev.getInfo<CL_DEVICE_MAX_COMPUTE_UNITS>(&cl_err);
    checkError(cl_err, "device compute_units");
    std::cerr << "Number of compute units: " << compute_units << "\n";

    auto clock_freq = dev.getInfo<CL_DEVICE_MAX_CLOCK_FREQUENCY>(&cl_err);
    checkError(cl_err, "device clock_freq");
    std::cerr << "Max clock freq.: " << clock_freq << " MHz\n";

    auto plat = cl::Platform(dev.getInfo<CL_DEVICE_PLATFORM>(&cl_err));
    checkError(cl_err, "device platform");
    auto plat_name = plat.getInfo<CL_PLATFORM_NAME>(&cl_err);
    checkError(cl_err, "device platform name");

    // Approximation, all nvidia except turing is 2 FLOPs
    auto FLOPs = 0;
    if (plat_name.compare(0, 6, "NVIDIA") == 0) {
      FLOPs = 2;
      // This may not be a robust way to find # of shaders!!!
      compute_units *= dev.getInfo<CL_DEVICE_WARP_SIZE_NV>(&cl_err) * 4;
    } else if (plat_name.compare(0, 3, "AMD") == 0) {
      FLOPs = 1;
      auto float_vec_width =
#ifndef CL_DEVICE_SIMD_PER_COMPUTE_UNIT_AMD
          4; // Value for Vega 64
#else
          dev.getInfo<CL_DEVICE_SIMD_PER_COMPUTE_UNIT_AMD>(&cl_err) / 2;
#endif
      compute_units *=
#ifndef CL_DEVICE_WAVEFRONT_WIDTH_AMD
          64; // Value for Vega 64
#else
          dev.getInfo<CL_DEVICE_WAVEFRONT_WIDTH_AMD>(&cl_err) * float_vec_width;
#endif
    } else { // Intel CPUs seem to scale with vector width:
             // https://en.wikipedia.org/wiki/FLOPS#FLOPs_per_cycle_for_various_processors
             // I'll have to test on AMD CPU and GPU and Intel GPU hardware to
             // know what I can use to get FLOPs
      auto float_vec_width =
          dev.getInfo<CL_DEVICE_NATIVE_VECTOR_WIDTH_FLOAT>(&cl_err);
      checkError(cl_err, "device float vec width");
      if (device_name == "Intel(R) Many Integrated Core Acceleration Card") {
        compute_units /= 4;
        // because the vector width of AVX512 and double hyperthreading cannot
        // happen at the same time
      }
      FLOPs = float_vec_width * 2; // 2 = (2 AVX2 + 2 FMA) / 2
    }

    std::cerr << "Max FLOPS: " << (clock_freq * compute_units * FLOPs) / 1000.f
              << " GFLOPS\n";

    std::cerr << "\n";
  }

  std::cerr << "FLOPS is calculated for single-precision float\n - if you know "
               "the number is wrong, let me know so I can adjust the "
               "calculation for your hardware\n";
  return 0;
}
