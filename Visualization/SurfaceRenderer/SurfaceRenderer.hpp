#ifndef SURFACERENDERER_HPP_
#define SURFACERENDERER_HPP_

#include "Renderer.hpp"
#include "ImageData.hpp"

namespace fast {

class SurfaceRenderer : public Renderer {
    FAST_OBJECT(SurfaceRenderer)
    public:
        void setInput(ImageData::pointer image);
        void setThreshold(float threshold);
        void keyPressEvent(QKeyEvent* event);
        void mouseMoveEvent(QMouseEvent* event, View* view);
        void resizeEvent(QResizeEvent* event);
    private:
        SurfaceRenderer();
        void execute();
        void draw();

        OpenCLDevice::pointer mDevice;
        ImageData::pointer mInput;

        float mThreshold;
        GLuint VBO_ID;
        bool mHasCreatedTriangles;
        unsigned int totalSum;
        float camX,camY,camZ;
        float rotationX,rotationY;
        //unsigned int windowWidth, windowHeight;
        float scalingFactorx, scalingFactory, scalingFactorz;
        float translationx, translationy, translationz;
        unsigned int HPSize;
        cl::Program program;
        // HP
        std::vector<cl::Image3D> images;
        std::vector<cl::Buffer> buffers;

        cl::Buffer cubeIndexesBuffer;
        cl::Image3D cubeIndexesImage;

        unsigned int mWidth, mHeight;
};

} // namespace fast




#endif /* SURFACERENDERER_HPP_ */