cmake_minimum_required(VERSION 3.20)
# LLVM's packaged CMake configuration probes C dependencies even though BTAP
# itself is C++. Enabling C keeps that discovery portable on Arch and elsewhere.
project(BTAP LANGUAGES C CXX)

option(BTAP_BUILD_PASS "Build the LLVM instrumentation plugin" ON)

add_library(btap_runtime STATIC runtime/btap_runtime.cpp)
target_compile_features(btap_runtime PUBLIC cxx_std_17)
target_include_directories(btap_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/runtime")

if(BTAP_BUILD_PASS)
  # LLVM_DIR normally looks like /usr/lib/cmake/llvm on Linux.  The plugin and
  # the clang/opt used with it must have the same LLVM major version.
  find_package(LLVM CONFIG QUIET)
  if(LLVM_FOUND)
    message(STATUS "Building BTAP against LLVM ${LLVM_PACKAGE_VERSION}")
    list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
    include(AddLLVM)
    add_llvm_pass_plugin(BTAPBoundsCheck passes/BoundsCheckPass.cpp)
    target_compile_features(BTAPBoundsCheck PRIVATE cxx_std_17)
    target_include_directories(BTAPBoundsCheck PRIVATE "${LLVM_INCLUDE_DIRS}")
  else()
    message(WARNING
      "LLVM development files were not found; only btap_runtime will be built. "
      "Install the LLVM development package matching clang and configure with "
      "-DLLVM_DIR=<llvm-cmake-directory>.")
  endif()
endif()
