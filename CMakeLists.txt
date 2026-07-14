cmake_minimum_required(VERSION 3.16)
project(elpc VERSION 0.1.0 LANGUAGES CXX)

# ----------------------------------------------------------------
# Core library — header only
# ----------------------------------------------------------------

add_library(elpc INTERFACE)
add_library(elpc::elpc ALIAS elpc)

target_include_directories(elpc
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(elpc INTERFACE cxx_std_20)
target_compile_definitions(elpc INTERFACE ELPC_VERSION="${PROJECT_VERSION}")

# ----------------------------------------------------------------
# Optional LLVM backend
# Enable with: cmake -DELPC_ENABLE_LLVM=ON ..
# ----------------------------------------------------------------

option(ELPC_ENABLE_LLVM "Enable LLVM backend support" OFF)

if(ELPC_ENABLE_LLVM)
  find_package(LLVM REQUIRED CONFIG)

  message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
  message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

  target_link_libraries(elpc INTERFACE LLVM)
  target_compile_definitions(elpc INTERFACE ELPC_ENABLE_LLVM)
endif()

# ----------------------------------------------------------------
# Install rules
# ----------------------------------------------------------------

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS elpc
    EXPORT elpcTargets
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT elpcTargets
    FILE      elpcTargets.cmake
    NAMESPACE elpc::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/elpc
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/elpcConfigVersion.cmake"
    VERSION      ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/elpcConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/elpcConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/elpc
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/elpcConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/elpcConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/elpc
)

# ----------------------------------------------------------------
# Tests — only built when elpc is the root project
# ----------------------------------------------------------------

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  function(add_elpc_test name)
    add_executable(${name} tests/${name}.cpp)
    target_link_libraries(${name} PRIVATE elpc)
  endfunction()

  add_elpc_test(TestLexer)
  add_elpc_test(TestParser)
  add_elpc_test(TestDiagnostics)
  add_elpc_test(TestSymbolTable)
  add_elpc_test(TestSema)
  add_elpc_test(TestIR)
  add_elpc_test(TestIR2)
  add_elpc_test(TestPratt)
  add_elpc_test(TestAST)
  add_elpc_test(TestInternedString)
  add_elpc_test(TestTypeSystem)
  add_elpc_test(TestFullPipeline)

  # ----------------------------------------------------------------
  # Mini example
  # ----------------------------------------------------------------
  add_executable(mini examples/mini/mini.cpp)
  target_link_libraries(mini PRIVATE elpc)

  add_executable(repl examples/repl/repl.cpp)
  target_link_libraries(repl PRIVATE elpc)

  add_executable(mini_ir examples/mini_ir/mini_ir.cpp)
  target_link_libraries(mini_ir PRIVATE elpc)

  add_executable(elpc-cli tools/elpc/elpc.cpp)
  target_link_libraries(elpc-cli PRIVATE elpc)

  if(ELPC_ENABLE_LLVM)
    add_elpc_test(TestLLVM)
  endif()
endif()
