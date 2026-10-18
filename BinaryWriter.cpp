#include "BinaryWriter.h"

static int write_uint32(std::ofstream& outFile, uint32_t value) {
	outFile.write(reinterpret_cast<const char*>(&value), sizeof(value));
	return 0;
}

static int write_uint16(std::ofstream& outFile, uint16_t value) {
	outFile.write(reinterpret_cast<const char*>(&value), sizeof(value));
	return 0;
}

static int write_uint8(std::ofstream& outFile, uint8_t value) {
	outFile.write(reinterpret_cast<const char*>(&value), sizeof(value));
	return 0;
}

int write_bmp(const std::string file_name, const uint8_t* begin, std::size_t image_width, std::size_t image_height) {
	std::ofstream file(file_name, std::ios::binary); // create an object called file from ofstream class

	if (!file) {
		std::cerr << "Failed to open output.\n";
		return 1;
	}

	// number of bytes of pixel data
	// Each entire row needs to have a multiple of four bytes in it, so there is padding.
	std::size_t image_size = (image_height * image_width * 3) + (image_height * ((4 - image_width % 4) % 4));
	std::size_t padding = (4 - (image_width * 3) % 4) % 4;

	// BMP file header
	uint32_t file_size = 14 + 40 + image_size; // file header + DIB header + array size
	uint16_t reserved = 0; // reserved 4 bytes
	uint32_t data_offset = 54; // byte address at which the actual image matrix begins

	// DIB file header
	uint32_t biSize = 40;
	uint32_t biWidth = image_width;
	uint32_t biHeight = image_height;
	uint16_t biPlanes = 1;
	uint16_t biBitCount = 24;
	uint32_t biCompression = 0;
	uint32_t biSizeImage = image_size;
	uint32_t biXPelsPerMeter = 0;
	uint32_t biYPelsPerMeter = 0;
	uint32_t biClrUsed = 0;
	uint32_t biClrImportant = 0;

	// header field to describe the file (BMP)
	file << "BM";
	write_uint32(file, file_size);
	write_uint16(file, reserved);
	write_uint16(file, reserved);
	write_uint32(file, data_offset);

	// write the DIB header
	write_uint32(file, biSize);
	write_uint32(file, biWidth);
	write_uint32(file, biHeight);
	write_uint16(file, biPlanes);
	write_uint16(file, biBitCount);
	write_uint32(file, biCompression);
	write_uint32(file, biSizeImage);
	write_uint32(file, biXPelsPerMeter);
	write_uint32(file, biYPelsPerMeter);
	write_uint32(file, biClrUsed);
	write_uint32(file, biClrImportant);

	uint8_t zero8 = 0;

	for (std::size_t row = 0; row < image_height; row++) {

		const uint8_t* src_row = begin + ((image_height - 1 - row) * image_width * 3);

		for (std::size_t col = 0; col < image_width; col++) {
			write_uint8(file, src_row[col * 3 + 0]);
			write_uint8(file, src_row[col * 3 + 1]);
			write_uint8(file, src_row[col * 3 + 2]);
		}

		for (std::size_t i = 0; i < padding; i++) {
			write_uint8(file, zero8);
		}
	}

	return 0;
}