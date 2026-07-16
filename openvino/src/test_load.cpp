// test_load.cpp - Sanity check: load an ONNX model with OpenVINO and print IO info.
// No encryption yet, this only validates the toolchain/environment.

#include <openvino/openvino.hpp>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << "model.onnx>\n";
        return 1;
    }

    // Create the OpenVINO runtime core.
    ov::Core core;
    std::cout <<"OpenVINO Core created OK\n";

    // Load the model from disk to validate the local OpenVINO setup.
    std::shared_ptr<ov::Model> model = core.read_model(argv[1]);
    std::cout <<"Model loaded: " << model->get_friendly_name() << "\n";

    // Print each input tensor name and shape.
    for (const auto& input : model->inputs()) {
        std::cout << "  Input: " << input.get_any_name()
                  << " shape: " << input.get_partial_shape() << "\n";
    }
    // Print each output tensor name and shape.
    for (const auto& output : model->outputs()) {
        std::cout << "  Output: " << output.get_any_name()
                    << " shape: " << output.get_partial_shape() << "\n";
    }

    return 0;
}