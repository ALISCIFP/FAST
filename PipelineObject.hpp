#ifndef PIPELINE_OBJECT_HPP
#define PIPELINE_OBJECT_HPP

#include <boost/shared_ptr.hpp>

class PipelineObject;
typedef boost::shared_ptr<PipelineObject> PipelineObjectPtr;

class PipelineObject {
    public:
        void update();
    protected:
        // Pointer to the parent pipeline object
        PipelineObjectPtr parentPipelineObject;

        // Flag to indicate whether the object has been modified
        // and should be executed again
        bool isModified;

        // Pure virtual method for executing the pipeline object
        virtual void execute()=0;
};

#endif
