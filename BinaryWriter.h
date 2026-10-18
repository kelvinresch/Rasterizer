// Rasterizer.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <iostream> // has cerr, which is an error stream object.
#include <fstream> // has std::ifstream and std::ofstream classes, as well as std:fstream
#include <cstdint> // has fixed-width integer types
#include <cstddef> // this supports size_t
#include <array> // this is to support modern C++ arrays
#include <string>

static int write_uint32(std::ofstream& outFile, uint32_t value);

static int write_uint16(std::ofstream& outFile, uint16_t value);

static int write_uint8(std::ofstream& outFile, uint8_t value);

int write_bmp(const std::string file_name, const uint8_t* begin, std::size_t ImageWidth, std::size_t ImageHeight);

// TODO: Reference additional headers your program requires here.
