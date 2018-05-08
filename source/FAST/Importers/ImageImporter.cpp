#include "ImageImporter.hpp"
#ifdef FAST_MODULE_VISUALIZATION
#include <QImage>
#endif
#include "FAST/Data/DataTypes.hpp"
#include "FAST/DeviceManager.hpp"
#include "FAST/Exception.hpp"
#include "FAST/Data/Image.hpp"
#include <cctype>
#include <algorithm>

namespace fast {

uchar* ImageImporter::readBMPFile(std::string filename, int &width, int &height) {
    FILE* img = fopen(filename.c_str(), "rb");
    if(img == NULL) {
        throw FileNotFoundException(filename);
    }

    // Read entire header of 54 bytes
    unsigned char header[54];
    std::size_t bytesRead = fread(header, sizeof(unsigned char), 54, img);


    // Get width and height
    width = *(int*)&header[18];
    height = *(int*)&header[22];
    reportInfo() << "Loaded image with size " << width << " "  << height << Reporter::end();

    // Calculate padding; the width as to be dividable by 4.
    // Padding is added at the end
    int padding=0;
    while((width*3+padding) % 4 != 0)
        padding++;

    int widthnew = width*3+padding;

    unsigned char* row = new unsigned char[widthnew];
    uchar* pixels = new uchar[width*height];

    for(int y = 0; y < height; y++) {
        // Read row
        std::size_t bytesRead = fread(row, sizeof(unsigned char), widthnew, img);
        if(bytesRead != widthnew)
            throw Exception("Error reading bmp image");

        // Convert BGR to RGB
        if(mGrayscale) {
            for(int x = 0; x < width; ++x) {
                pixels[y*width+x] = (uchar)round((row[x*3] + row[x*3+1] + row[x*3+2])/3.0f);
            }
        } else {
            for(int x = 0; x < width; ++x) {
                pixels[y * width + x + 0] = row[x + 2];
                pixels[y * width + x + 1] = row[x + 1];
                pixels[y * width + x + 2] = row[x + 0];
            }
        }
    }

    delete[] row;
    fclose(img); //close the file
    return pixels;
}

void ImageImporter::execute() {
    if (mFilename == "")
        throw Exception("No filename was supplied to the ImageImporter");

    int width;
    int height;
    uchar* convertedPixelData;
#ifdef FAST_MODULE_VISUALIZATION
    // Load image from disk using Qt
    QImage image;
    reportInfo() << "Trying to load image..." << Reporter::end();
    if(!image.load(mFilename.c_str())) {
        throw FileNotFoundException(mFilename);
    }
    reportInfo() << "Loaded image with size " << image.width() << " "  << image.height() << Reporter::end();

    QImage::Format format;
    if(mGrayscale) {
        format = QImage::Format_Grayscale8;
    } else {
        format = QImage::Format_RGB888;
    }
    QImage convertedImage = image.convertToFormat(format);

    // Get pixel data
    convertedPixelData = convertedImage.bits();

    width = image.width();
    height = image.height();
#else
    std::string extension = mFilename.substr(mFilename.rfind(".") + 1);
    // Convert to lower case
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    if(extension == "bmp") {
        // Use built in BMP loader
        convertedPixelData = readBMPFile(mFilename, width, height);
    } else {
        throw Exception("Error reading " + mFilename + ". The ImageImporter need Qt for importing images other than BMP, but the visualization module is disabled");
    }
#endif

    // Transfer to texture(if OpenCL) or copy raw pixel data (if host)
    Image::pointer output = getOutputData<Image>();
    output->create(
            width,
            height,
            TYPE_UINT8,
            mGrayscale ? 1 : 3,
            getMainDevice(),
            convertedPixelData
    );
#ifndef FAST_MODULE_VISUALIZATION
    delete[] convertedPixelData;
#endif
}

ImageImporter::ImageImporter() {
    mFilename = "";
    mIsModified = true;
    mGrayscale = true;
    createOutputPort<Image>(0);
}

void ImageImporter::setGrayscale(bool grayscale) {
    mGrayscale = grayscale;
}

void ImageImporter::setFilename(std::string filename) {
    mFilename = filename;
    mIsModified = true;
}

}
