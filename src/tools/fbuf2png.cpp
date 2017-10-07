#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <png.h>
#include "../frontend/options.h"

static void write_to_stream(png_structp png_ptr, png_bytep data, png_size_t length) {
    png_voidp a = png_get_io_ptr(png_ptr);
    ((std::ostream*)a)->write((const char*)data, length);
}

static void flush_stream(png_structp png_ptr) {
    // Nothing to do
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "No arguments. Exiting." << std::endl;
        return EXIT_FAILURE;
    }

    int width, height;
    bool normalize;
    ArgParser parser(argc, argv);
    parser.add_option<int>("width", "w", "Sets the image width", width, 1024, "pixels");
    parser.add_option<int>("height", "h", "Sets the image height", height, 1024, "pixels");
    parser.add_option<bool>("normalize", "n", "Normalizes the resulting image.", normalize, false, "");

    if (!parser.parse()) {
        parser.usage();
        return EXIT_SUCCESS;
    }

    if (parser.arguments().size() < 2) {
        std::cerr << "Input file and output file expected. Exiting." << std::endl;
        return EXIT_FAILURE;
    } else if (parser.arguments().size() > 2) {
        std::clog << "Additional arguments ignored (";
        for (int i = 2; i < parser.arguments().size(); i++) {
            std::clog << "\'" << parser.arguments()[i] << "'";
            if (i != parser.arguments().size() - 1) std::clog << ", ";
        }
        std::clog << ")." << std::endl;
    }

    std::ifstream fbuf_file(parser.arguments()[0], std::ofstream::binary);
    std::ofstream png_file(parser.arguments()[1], std::ofstream::binary);
    if (!fbuf_file || !png_file)
        return EXIT_FAILURE;

    // Read fbuf file and convert it to an image
    std::vector<float> float_image(width * height);
    fbuf_file.read((char*)float_image.data(), width * height * sizeof(float));
    const float tmax = normalize ? *std::max_element(float_image.begin(), float_image.end()) : 1.0f;

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png_ptr) {
        return EXIT_FAILURE;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, nullptr, nullptr);
        return EXIT_FAILURE;
    }

    std::vector<uint8_t> row(width * 4);
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return EXIT_FAILURE;
    }

    png_set_write_fn(png_ptr, &png_file, write_to_stream, flush_stream);

    png_set_IHDR(png_ptr, info_ptr, width, height,
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t c = 255.0f * float_image[y * width + x] / tmax;
            row[x * 4 + 0] = c;
            row[x * 4 + 1] = c;
            row[x * 4 + 2] = c;
            row[x * 4 + 3] = 255.0f;
        }
        png_write_row(png_ptr, row.data());
    }

    png_write_end(png_ptr, info_ptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);

    return EXIT_SUCCESS;
}
