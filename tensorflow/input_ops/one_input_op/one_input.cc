#include "/usr/common/software/tensorflow/intel-tensorflow/1.8.0-py27/lib/python2.7/site-packages/tensorflow/include/tensorflow/core/framework/op.h"
#include "/usr/common/software/tensorflow/intel-tensorflow/1.8.0-py27/lib/python2.7/site-packages/tensorflow/include/tensorflow/core/framework/op_kernel.h"
#include "/usr/common/software/tensorflow/intel-tensorflow/1.8.0-py27/lib/python2.7/site-packages/tensorflow/include/tensorflow/core/framework/shape_inference.h"

#include "../../../core/core_c.h"

using namespace tensorflow;

REGISTER_OP("OneInput")
	.Input("first_input: T")
	.Input("task_graph: uint32")
	.Input("timestep: int32")
	.Input("point: int32")
	.Attr("T: {int8, int16, int32, int64}")
	.Output("output: T");

template <typename T>
class OneInputOp : public OpKernel {
public:

	explicit OneInputOp(OpKernelConstruction* context) : OpKernel(context) {}

	void Compute(OpKernelContext* context) override {
		// get input tensors
		const Tensor& first_input_tensor = context->input(0);
		const Tensor& timestep_tensor = context->input(2); // timestep where this op is being run
		const Tensor& point_tensor = context->input(3); // point where this op is being run
		// convert tensors to readable arrays
		auto first_input = first_input_tensor.flat<T>();
		auto timestep_input = timestep_tensor.flat<int32>();
		auto point_input = point_tensor.flat<int32>();
		long timestep = timestep_input(0);
		long point = point_input(0);

		// construct task_graph from given values
		const Tensor& task_graph_tensor = context->input(1);
		task_graph_t task_graph = constructTaskGraph(task_graph_tensor);

		TensorShape output_shape;
		output_shape.AddDim(2);

		Tensor* output_tensor = NULL;
		OP_REQUIRES_OK(context, context->allocate_output(0, output_shape, &output_tensor));
		auto output_flat = output_tensor->flat<T>(); // set output type to any of int8, int16, int32, int64

		std::pair<long, long> first_input_pair = std::make_pair(first_input(0), first_input(1));

		const std::pair<long, long> *input[1];
		input[0] = &first_input_pair;

		output_flat(0) = timestep;
		output_flat(1) = point;

		// Execute Point
		char *output_ptr = (char *)(&output_flat);
		size_t output_bytes = sizeof(output_flat);
		size_t n_inputs = 1;
		size_t input_bytes[n_inputs];
		input_bytes[0] = sizeof(*input[0]);
		const char **input_ptr = (const char **)(input);
		
		task_graph_execute_point(task_graph, timestep, point, output_ptr, output_bytes, input_ptr, input_bytes, n_inputs);
	}

private:

	task_graph_t constructTaskGraph(const Tensor& task_graph_tensor) {
		auto tg = task_graph_tensor.flat<uint32>();

		kernel_t kernel = { kernel_type_t(tg(4)), tg(5) };
		task_graph_t task_graph = { tg(0), tg(1), dependence_type_t(tg(2)), tg(3), kernel, tg(6), tg(7) };
		return task_graph;
	}

};

// REGISTER_KERNEL_BUILDER(Name("OneInput").Device(DEVICE_CPU), OneInputOp);

// Macro for registering operator for different types (int8, int16, int32, int64)
#define REGISTER_KERNEL(type)											\
	REGISTER_KERNEL_BUILDER(											\
		Name("OneInput").Device(DEVICE_CPU).TypeConstraint<type>("T"),	\
		OneInputOp<type>)

REGISTER_KERNEL(int8);
REGISTER_KERNEL(int16);
REGISTER_KERNEL(int32);
REGISTER_KERNEL(int64);

#undef REGISTER_KERNEL
