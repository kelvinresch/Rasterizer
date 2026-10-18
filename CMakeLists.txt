# CMakeList.txt : CMake project for Rasterizer, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("Rasterizer")

find_package(SDL2 CONFIG REQUIRED)

# Add source to this project's executable.
add_executable (Rasterizer "Rasterizer.cpp" "Rasterizer.h" "BinaryWriter.cpp" "BinaryWriter.h")

target_link_libraries(Rasterizer PRIVATE SDL2::SDL2main SDL2::SDL2)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET Rasterizer PROPERTY CXX_STANDARD 20)
endif()

# TODO: Add tests and install targets if needed.
