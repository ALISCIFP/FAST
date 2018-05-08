#include <FAST/Testing.hpp>
#include <FAST/Streamers/MovieStreamer.hpp>
#include <FAST/Visualization/ImageRenderer/ImageRenderer.hpp>
#include <FAST/Visualization/SimpleWindow.hpp>

using namespace fast;

TEST_CASE("MovieStreamer", "[fast][moviestreamer]") {
    MovieStreamer::pointer streamer = MovieStreamer::New();
    streamer->setFilename("/home/smistad/Desktop/axillary_block.mp4");

    ImageRenderer::pointer renderer = ImageRenderer::New();
    renderer->addInputConnection(streamer->getOutputPort());

    SimpleWindow::pointer window = SimpleWindow::New();
    window->addRenderer(renderer);
    window->set2DMode();
    window->setTimeout(1000);
    window->start();

}