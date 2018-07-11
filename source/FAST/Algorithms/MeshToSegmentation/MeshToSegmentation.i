%include "FAST/ProcessObject.i"
%shared_ptr(fast::SegmentationAlgorithm)
%shared_ptr(fast::MeshToSegmentation)

namespace fast {

%ignore SegmentationAlgorithm;
class SegmentationAlgorithm : public ProcessObject {

};
%template(SegmentationAlgorithmPtr) SharedPointer<SegmentationAlgorithm>;

class MeshToSegmentation : public SegmentationAlgorithm {
    public:
    	static SharedPointer<MeshToSegmentation> New();
        /**
         * Set output image resolution in voxels
         * @param x
         * @param y
         * @param z
         */
		void setOutputImageResolution(uint x, uint y, uint z = 1);
	private:
		MeshToSegmentation();
};

// This must come after class declaration
%template(MeshToSegmentationPtr) SharedPointer<MeshToSegmentation>;
}
