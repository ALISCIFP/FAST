#include "Image2D.hpp"
#include "HelperFunctions.hpp"
using namespace fast;



void Image2D::setOpenCLImage(cl::Image2D *clImage, OpenCLDevice::pointer device) {
    mCLImages.push_back(clImage);
    mCLDevices.push_back(device);
    mWidth = clImage->getImageInfo<CL_IMAGE_WIDTH>();
    mHeight = clImage->getImageInfo<CL_IMAGE_HEIGHT>();
}

OpenCLImageAccess2D Image2D::getOpenCLImageAccess(
        accessType type,
        OpenCLDevice::pointer device) {
    // TODO: Check for write access

    // Check to see if image exist on the device
    for(unsigned int i = 0; i < mCLDevices.size(); i++) {
        if(mCLDevices[i] == device) {
            return OpenCLImageAccess2D(mCLImages[i]);
        }
    }

    // TODO: Transfer data to a new device
}

Image2D::~Image2D() {
    // Delete all images
    for(unsigned int i = 0; i < mCLImages.size(); i++) {
        delete mCLImages[i];
    }

    delete[] (float*)mHostData;
}

Image2D::Image2D() {
    mHostData = NULL;
}

ImageAccess2D Image2D::getImageAccess(accessType type) {
    // TODO: this method is currently just a fixed hack
    // TODO: Check for write access

    // TODO Check to see if data exist on host
    mHostData = new float[mWidth*mHeight];

    if(mCLImages.size() > 0) {
    // If not retrieve it
    mCLDevices[0]->getCommandQueue().enqueueReadImage(
            *mCLImages[0],
            CL_TRUE,
            oul::createOrigoRegion(),
            oul::createRegion(mWidth,mHeight,1),
            0,0,
            mHostData
            );
    }

    return ImageAccess2D(mHostData);
}

void Image2D::createImage(unsigned int width, unsigned int height) {
    mWidth = width;
    mHeight = height;
}
