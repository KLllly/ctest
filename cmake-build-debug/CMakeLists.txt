cmake_minimum_required(VERSION 3.15)
project(cmake_build_debug)

set(CMAKE_CXX_STANDARD 11)

add_executable(cmake_build_debug
        3rd_party/argparse/argparse.cbp
        3rd_party/argparse/cmake_install.cmake
        3rd_party/argparse/Makefile
        3rd_party/catch2/catch2.cbp
        3rd_party/catch2/cmake_install.cmake
        3rd_party/catch2/Makefile
        3rd_party/fmt/cmake_install.cmake
        3rd_party/fmt/FMT.cbp
        3rd_party/fmt/libfmtd.a
        3rd_party/fmt/Makefile
        miniplc0.dSYM/Contents/Resources/DWARF/miniplc0
        miniplc0.dSYM/Contents/Info.plist
        cmake_install.cmake
        CMakeCache.txt
        CTestTestfile.cmake
        hello.plc0
        libminiplc0_lib.a
        Makefile
        miniplc0
        miniplc0.cbp)
