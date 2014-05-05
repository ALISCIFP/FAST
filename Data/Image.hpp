#ifndef IMAGE2D_HPP
#define IMAGE2D_HPP

#include "ImageData.hpp"
#include "DataTypes.hpp"
#include "SmartPointers.hpp"
#include "OpenCLManager.hpp"
#include "ExecutionDevice.hpp"
#include "OpenCLImageAccess2D.hpp"
#include "OpenCLImageAccess3D.hpp"
#include "ImageAccess2D.hpp"
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
namespace fast {

class Image: public ImageData {
    FAST_OBJECT(Image)
    public:
        void create2DImage(unsigned int width, unsigned int height, DataType type, unsigned int nrOfComponents, ExecutionDevice::pointer device);
        void create2DImage(unsigned int width, unsigned int height, DataType type, unsigned int nrOfComponents, ExecutionDevice::pointer device, const void * data);
        void create3DImage(unsigned int width, unsigned int height, unsigned int depth, DataType type, unsigned int nrOfComponents, ExecutionDevice::pointer device);
        void create3DImage(unsigned int width, unsigned int height, unsigned int depth, DataType type, unsigned int nrOfComponents, ExecutionDevice::pointer device, const void * data);

        // TODO add 3D support to these methods
        OpenCLImageAccess2D getOpenCLImageAccess2D(accessType type, OpenCLDevice::pointer);
        OpenCLImageAccess3D getOpenCLImageAccess3D(accessType type, OpenCLDevice::pointer);
        ImageAccess2D getImageAccess(accessType type);

        ~Image() { freeAll(); };

        unsigned int getWidth() const;
        unsigned int getHeight() const;
        unsigned int getDepth() const;
        unsigned char getDimensions() const;
        DataType getDataType() const;
        unsigned int getNrOfComponents() const;
    private:
        Image();

        // TODO add support for OpenCL buffers as well
        boost::unordered_map<OpenCLDevice::pointer, cl::Image*> mCLImages;
        boost::unordered_map<OpenCLDevice::pointer, bool> mCLImagesIsUpToDate;
        boost::unordered_map<OpenCLDevice::pointer, bool> mCLImagesAccess;
        void * mHostData;
        bool mHostHasData;
        bool mHostDataIsUpToDate;
        bool mHostDataIsBeingAccessed;
        bool isDataModified();
        void updateOpenCLImageData(OpenCLDevice::pointer device);
        void updateHostData();
        void setAllDataToOutOfDate();
        bool isAnyDataBeingAccessed();
        void transferCLImageFromHost(OpenCLDevice::pointer device);
        void transferCLImageToHost(OpenCLDevice::pointer device);
        bool isInitialized();
        void free(ExecutionDevice::pointer device);
        void freeAll();

        unsigned int mWidth, mHeight, mDepth;
        unsigned char mDimensions;
        DataType mType;
        unsigned int mComponents;
};

} // end namespace fast

#endif