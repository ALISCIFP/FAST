#include "NeuralNetwork.hpp"
#include "FAST/Data/Image.hpp"
#include "FAST/Algorithms/ImageResizer/ImageResizer.hpp"

#include <tensorflow/core/framework/step_stats.pb.h>
#include <tensorflow/core/framework/tensor.h>
#include <tensorflow/core/framework/types.pb.h>
#include <tensorflow/core/lib/strings/stringprintf.h>
#include <tensorflow/core/platform/env.h>
#include <tensorflow/core/platform/logging.h>
#include <tensorflow/core/platform/mutex.h>
#include <tensorflow/core/platform/types.h>
#include <tensorflow/core/public/session.h>
#include <tensorflow/core/graph/default_device.h>
#include <tensorflow/core/platform/init_main.h>
#include <tensorflow/cc/framework/ops.h>

namespace fast {

// See here for reference: https://github.com/tensorflow/tensorflow/blob/86f5ab7474825da756838b34e1b4eac93f5fc68a/tensorflow/contrib/android/jni/tensorflow_inference_jni.cc

void NeuralNetwork::load(std::string networkFilename) {

	char** argv = new char*[1];
	argv[0] = new char[255];
    int argc = 1;
    tensorflow::port::InitMain(argv[0], &argc, &argv);
	tensorflow::SessionOptions options;
	tensorflow::ConfigProto &config = options.config;
	tensorflow::GPUOptions* gpuOptions = config.mutable_gpu_options();
	gpuOptions->set_allow_growth(true); // Set this so that tensorflow will not use up all GPU memory
	//gpuOptions->set_per_process_gpu_memory_fraction(0.5);
	mSession.reset(tensorflow::NewSession(options));
	tensorflow::GraphDef tensorflow_graph;

	{
		tensorflow::Status s = ReadBinaryProto(tensorflow::Env::Default(), networkFilename, &tensorflow_graph);
		if (!s.ok()) {
			throw Exception("Could not read TensorFlow graph file " + networkFilename);
		}
	}

	// Assume first node is input node
	if(mInputName == "")
        mInputName = tensorflow_graph.node(0).name();
    for(int i = 0; i < tensorflow_graph.node_size(); ++i) {
		tensorflow::NodeDef node = tensorflow_graph.node(i);
        //reportInfo() << "Node " << i << " with name " << node.name() << reportEnd();
        //reportInfo() << "Op name " << node.op() << reportEnd();
        //reportInfo() << "inputs: " << node.input_size() << reportEnd();
        if(node.name().find("keras_learning_phase") != std::string::npos) {
			mLearningPhaseTensors.push_back(node.name());
		}
	}

	reportInfo() << "Creating session." << reportEnd();
	tensorflow::Status s = mSession->Create(tensorflow_graph);
	if (!s.ok()) {
		throw Exception("Could not create TensorFlow Graph");
	}

	//tensorflow::graph::SetDefaultDevice("/gpu:0", &tensorflow_graph);

	// Clear the proto to save memory space.
	tensorflow_graph.Clear();
	reportInfo() << "TensorFlow graph loaded from: " << networkFilename << reportEnd();

	mModelLoaded = true;
}

void NeuralNetwork::setScaleFactor(float factor) {
    mScaleFactor = factor;
}

void NeuralNetwork::setPreserveAspectRatio(bool preserve) {
    mPreserveAspectRatio = preserve;
}

void NeuralNetwork::setHorizontalFlipping(bool flip) {
	mHorizontalImageFlipping = flip;
}

NeuralNetwork::NeuralNetwork() {
	createInputPort<Image>(0);
	mModelLoaded = false;
	mPreserveAspectRatio = false;
	mInputName = "";
	mWidth = -1;
	mHeight = -1;
	mScaleFactor = 1.0f;
	createOpenCLProgram(Config::getKernelSourcePath() + "Algorithms/NeuralNetwork/NeuralNetwork.cl");
	createStringAttribute("model", "Model path", "Path to neural network tensorflow model", "");
	createIntegerAttribute("input_size", "Input size", "Image input size", 128);
	createFloatAttribute("scale_factor", "Scale factor", "Scale factor", mScaleFactor);
	createStringAttribute("output_names", "Output names", "Name of output nodes", "");
	createBooleanAttribute("signed_input_normalization", "Signed input normalization", "Normalize input to -1 and 1 instead of 0 to 1.", false);
}

void NeuralNetwork::execute() {
    Image::pointer image = getInputData<Image>();

	mImages.push_back(image);
    while(mImages.size() < mFramesToRemember)
		mImages.push_back(image);
	while(mImages.size() > mFramesToRemember)
		mImages.pop_front();

	std::vector<Image::pointer> images;
    for(int i = 0; i < mImages.size(); ++i)
		images.push_back(mImages[i]);

	if(mWidth < 0 || mHeight < 0)
		throw Exception("Network input layer width and height has to be specified before running the network");

    images = resizeImages(images);

	executeNetwork(images);
}


void NeuralNetwork::setInputSize(int width, int height) {
	mWidth = width;
	mHeight = height;
}
void NeuralNetwork::setOutputParameters(std::vector<std::string> outputNodeNames) {
    mOutputNames = outputNodeNames;
}

tensorflow::Tensor NeuralNetwork::getNetworkOutput() {
    if(mOutputNames.size() != 1)
		throw Exception("If network has more than 1 output can't return network output without name.");

	return mOutputData[mOutputNames[0]];
}

tensorflow::Tensor NeuralNetwork::getNetworkOutput(std::string name) {
	return mOutputData.at(name);
}

void NeuralNetwork::executeNetwork(const std::vector<Image::pointer>& images) {
    if(!mModelLoaded)
		throw Exception("Network and weights must be loaded in NeuralNetwork before execution.");
	if(mInputName == "")
		throw Exception("An input name must ge given to the NeuralNetwork before execution");
	if(mOutputNames.size() == 0)
		throw Exception("An output name must ge given to the NeuralNetwork before execution");

    int batchSize = 1;//images.size();
	if(batchSize == 0)
		throw Exception("Need at least one image to execute network.");

	// Create input tensor
	tensorflow::TensorShape shape = tensorflow::TensorShape({batchSize, mHeight, mWidth, 1});
	if(mFramesToRemember > 1)
		shape = tensorflow::TensorShape({batchSize, mFramesToRemember, mHeight, mWidth, 1});

    mRuntimeManager->startRegularTimer("input_data_copy");
    float* values = new float[batchSize*mWidth*mHeight];

	OpenCLDevice::pointer device = getMainDevice();
    cl::Program program = getOpenCLProgram(device);
	cl::Kernel kernel(program, "normalizeInput");
	Image::pointer image = images[0];
	OpenCLImageAccess::pointer access = image->getOpenCLImageAccess(ACCESS_READ, device);
    cl::Buffer buffer(
			device->getContext(),
			CL_MEM_WRITE_ONLY,
			sizeof(float)*mWidth*mHeight
	);
	kernel.setArg(0, *access->get2DImage());
	kernel.setArg(1, buffer);
	kernel.setArg(2, mScaleFactor);
	kernel.setArg(3, (int)(mHorizontalImageFlipping ? 1 : 0));
	kernel.setArg(4, (int)(mSignedInputNormalization ? 1 : 0));

	device->getCommandQueue().enqueueNDRangeKernel(
			kernel,
			cl::NullRange,
			cl::NDRange(mWidth, mHeight),
			cl::NullRange
	);

	device->getCommandQueue().enqueueReadBuffer(buffer, CL_TRUE, 0, sizeof(float)*mWidth*mHeight, values);

	tensorflow::Tensor input_tensor(
			tensorflow::DT_FLOAT,
			shape
	);
    if(image->getWidth() != mWidth || image->getHeight() != mHeight)
        throw Exception("Input image sent to executeNetwork was of incrorrect size");

	auto input_tensor_mapped = input_tensor.tensor<float, 4>();
    for(int i = 0; i < mHeight; ++i) { // y
        for(int j = 0; j < mWidth; ++j) { // x
			input_tensor_mapped(0, i, j, 0) = values[j+i*mWidth];
        }
    }
    delete[] values;

	/*
    if(mFramesToRemember > 1) {
        auto input_tensor_mapped = input_tensor.tensor<float, 5>();

		mRuntimeManager->startRegularTimer("input_data_copy");
		reportInfo() << "TensorFlow: Copying Data." << reportEnd();
		for(int n = 0; n < batchSize; ++n) {
            for(int t = 0; t < images.size(); ++t) {
				Image::pointer image = images[t];
				if(image->getWidth() != mWidth || image->getHeight() != mHeight)
					throw Exception("Input image sent to executeNetwork was of incrorrect size");

				ImageAccess::pointer access = image->getImageAccess(ACCESS_READ);
				if(mHorizontalImageFlipping) {
					for(int i = 0; i < mHeight; ++i) { // y
						for(int j = 0; j < mWidth; ++j) { // x
							input_tensor_mapped(n, t, i, mWidth - j - 1, 0) =
                                access->getScalar(Vector2i(j, i)) * mScaleFactor;
						}
					}
				} else {
					for(int i = 0; i < mHeight; ++i) { // y
						for(int j = 0; j < mWidth; ++j) { // x
							input_tensor_mapped(n, t, i, j, 0) = access->getScalar(Vector2i(j, i)) * mScaleFactor;
						}
					}
				}
			}
		}
	} else {
		auto input_tensor_mapped = input_tensor.flat<float>();

		mRuntimeManager->startRegularTimer("input_data_copy");
		reportInfo() << "TensorFlow: Copying Data." << reportEnd();
		for(int n = 0; n < batchSize; ++n) {
			Image::pointer image = images[n];
			if(image->getWidth() != mWidth || image->getHeight() != mHeight)
				throw Exception("Input image sent to executeNetwork was of incrorrect size");

			ImageAccess::pointer access = image->getImageAccess(ACCESS_READ);
			if(mHorizontalImageFlipping) {
				for(int i = 0; i < mHeight; ++i) { // y
					for(int j = 0; j < mWidth; ++j) { // x
						input_tensor_mapped(n + i + (mWidth - j - 1)*mHeight) = access->getScalar(Vector2i(j, i)) * mScaleFactor;
					}
				}
			} else {
				for(int i = 0; i < mHeight; ++i) { // y
					for(int j = 0; j < mWidth; ++j) { // x
						input_tensor_mapped(n, i, j, 0) = access->getScalar(Vector2i(j, i)) * mScaleFactor;
					}
				}
			}
		}
	}
	 */
	mRuntimeManager->stopRegularTimer("input_data_copy");

    // TODO Need to know names of inputs and outputs in advance
	// Input: Only single for now
	// Output: Can be multiple


	std::vector <std::pair<std::string, tensorflow::Tensor>> input_tensors(
			{{mInputName, input_tensor}});

    for(std::string name : mLearningPhaseTensors) {
        // Create a scalar tensor which tells the system we are NOT doing training
        tensorflow::Tensor input_tensor2(
                tensorflow::DT_BOOL,
                tensorflow::TensorShape() // Scalar
        );
        auto input_tensor_mapped2 = input_tensor2.tensor<bool, 0>();
        input_tensor_mapped2(0) = false;
        input_tensors.push_back(std::make_pair(name, input_tensor2));
    }

	std::vector <tensorflow::Tensor> output_tensors;

	reportInfo() << "Running network" << reportEnd();
	tensorflow::Status s;
	mRuntimeManager->startRegularTimer("network_execution");
	s = mSession->Run(input_tensors, mOutputNames, {}, &output_tensors);
	mRuntimeManager->stopRegularTimer("network_execution");

	if (!s.ok()) {
		throw Exception("Error during inference: " + s.ToString());
	}
	reportInfo() << "Finished executing network" << reportEnd();
    // Store all output data
    for(int j = 0; j < mOutputNames.size(); ++j) {
        std::string outputName = mOutputNames[j];
		mOutputData[outputName] = output_tensors[j];
	}
	reportInfo() << "Finished parsing output" << reportEnd();

}

std::vector<SharedPointer<Image>> NeuralNetwork::resizeImages(const std::vector<SharedPointer<Image>> &images) {
	reportInfo() << "Resizing images.." << reportEnd();
    std::vector<Image::pointer> resizedImages;
	for(Image::pointer image : images) {
		// Resize image to fit input layer
		if(mWidth != image->getWidth() || mHeight != image->getHeight()) {
			// Only resize if needed
            ImageResizer::pointer resizer = ImageResizer::New();
            resizer->setWidth(mWidth);
            resizer->setHeight(mHeight);
            resizer->setInputData(image);
			resizer->setPreserveAspectRatio(mPreserveAspectRatio);
			DataPort::pointer port = resizer->getOutputPort();
            resizer->update(0);
            Image::pointer resizedImage = port->getNextFrame();
            mNewInputSpacing = resizedImage->getSpacing();
            resizedImages.push_back(resizedImage);
		} else {
			mNewInputSpacing = image->getSpacing();
			resizedImages.push_back(image);
		}
	}

	return resizedImages;
}

void NeuralNetwork::loadAttributes() {
	load(getStringAttribute("model"));
	std::vector<int> inputSize = getIntegerListAttribute("input_size");
	setInputSize(inputSize.at(0), inputSize.at(1));
	std::vector<std::string> outputNames = getStringListAttribute("output_names");
	setOutputParameters(outputNames);
	setScaleFactor(getFloatAttribute("scale_factor"));
	setSignedInputNormalization(getBooleanAttribute("signed_input_normalization"));
}

void NeuralNetwork::setRememberFrames(uint nrOfFrames) {
	if(nrOfFrames < 1) {
        throw Exception("Remember frames has to be > 0.");
	}
	mFramesToRemember = nrOfFrames;
}

void NeuralNetwork::setInputName(std::string inputName) {
	mInputName = inputName;
}

void NeuralNetwork::setSignedInputNormalization(bool signedInputNormalization) {
	mSignedInputNormalization = signedInputNormalization;
}

};
