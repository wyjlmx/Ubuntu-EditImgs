#include "Pipeline.hpp"
#include "mimgs/Mimgs.hpp"
#include "pipeline/OperatorRegistry.hpp"
#include "process/ScaleOperator.hpp"
#include "process/RotateOperator.hpp"
#include "process/PuzzleOperator.hpp"

namespace ei::mimgs
{
    void execute(IContext &context, const PipelineParams &params)
    {
        auto &registry = OperatorRegistry::getInstance();

        for(const auto &stepParams : params.steps)
        {
            if(!stepParams)
                throw ProcessingException("Pipeline::execute: Encountered null step parameters.");

            std::string opName;
            if(dynamic_cast<const ScaleParams*>(stepParams.get()))
                opName = "scale";
            else if(dynamic_cast<const RotateParams*>(stepParams.get()))
                opName = "rotate";
            else if(dynamic_cast<const PuzzleParams*>(stepParams.get()))
                opName = "puzzle";
            else
                throw ProcessingException("Pipeline::execute: Unknown/unsupported operator parameters type.");

            IOperator *op = registry.getOperator(opName);
            if(!op)
                throw ProcessingException("Pipeline::execute: Operator '" + opName + "' is not registered.");
            
            stepParams->Validate();
            op->execute(context, *stepParams);
        }
    }
}