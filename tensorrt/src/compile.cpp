// compile.cpp
//
// Purpose:
//  - Build a TensorRT engine from an ONNX model, then encrypt and write the
//    serialized engine to disk. This is a small CLI utility used in the
//    toolkit to produce encrypted engine blobs consumed by the runtime.
//
// High-level flow:
//  1. Parse ONNX file into a TensorRT network using the ONNX parser.
//  2. Build and serialize the network into an engine blob.
//  3. Derive an AES-256 key from a password using SHA-256, encrypt the blob
//     with AES-256-ECB, and write the ciphertext to the output path.
//

#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

// Simple TensorRT logger that prints WARNING and above to stderr.
class TRTLogger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING)
            std::cerr << "[TRT] " << msg << "\n";
    }
};

// Build a serialized TensorRT engine from an ONNX file path.
// Returns the serialized engine bytes as a vector<unsigned char>.
static std::vector<unsigned char> buildEngine(const std::string& onnxPath) {
    TRTLogger logger;

    // Create builder and network objects using unique_ptr for RAII.
    auto builder = std::unique_ptr<nvinfer1::IBuilder>(
        nvinfer1::createInferBuilder(logger));

    // createNetworkV2 expects flags; using 0 for default behavior.
    nvinfer1::INetworkDefinition* rawNetwork = builder->createNetworkV2(0);

    // Create an ONNX parser that populates the network from the ONNX bytes.
    auto parser = std::unique_ptr<nvonnxparser::IParser>(
        nvonnxparser::createParser(*rawNetwork, logger));

    // Read the ONNX file into memory.
    std::ifstream f(onnxPath, std::ios::binary | std::ios::ate);
    if (!f)
    {
        std::cerr << "Impossible to open : " << onnxPath << "\n"; exit(1);
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<char> onnxData(size);
    f.read(onnxData.data(), size);

    // Parse the ONNX model into the network. Parser returns false on errors.
    if (!parser->parse(onnxData.data(), onnxData.size())) {
        std::cerr << "Parsing error\n";
        exit(1);
    }

    // Build configuration for the builder and request detailed profiling info.
    auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(
        builder->createBuilderConfig());
    config->setProfilingVerbosity(nvinfer1::ProfilingVerbosity::kDETAILED);

    // Build and serialize the network into host memory.
    auto serialized = std::unique_ptr<nvinfer1::IHostMemory>(
        builder->buildSerializedNetwork(*rawNetwork, *config));
    if (!serialized)
    {
        std::cerr << "Build error\n"; exit(1);
    }

    // Copy serialized bytes into a convenient vector and clean up.
    const auto* ptr = static_cast<const unsigned char*>(serialized->data());
    std::vector<unsigned char> result(ptr, ptr + serialized->size());

    serialized.reset();
    config.reset();
    parser.reset();
    delete rawNetwork;
    builder.reset();

    return result;
}

// Encrypt `data` with a key derived from `password` using SHA-256 -> AES-256-ECB.
// Returns ciphertext bytes. See security notes above for warnings about ECB.
static std::vector<unsigned char> encrypt(const std::vector<unsigned char>& data,
const std::string& password) {
    unsigned char key[32];

    // Derive a 256-bit key by hashing the password with SHA-256.
    SHA256(reinterpret_cast<const unsigned char*>(password.data()),
    password.size(), key);

    // Initialize OpenSSL EVP context for AES-256-ECB encryption.
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    // Allocate output buffer with some extra space for padding.
    std::vector<unsigned char> out(data.size() + 32);
    int outLen1 = 0, outLen2 = 0;

    // Encrypt update + final to produce complete ciphertext.
    EVP_EncryptUpdate(ctx, out.data(), &outLen1,
    data.data(), static_cast<int>(data.size()));
    EVP_EncryptFinal_ex(ctx, out.data() + outLen1, &outLen2);
    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen1 + outLen2);
    return out;
}

// Small CLI that expects: <model.onnx> <model.engine.enc> <password>
int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
        << " <model.onnx> <model.engine.enc> <clé>\n";
        return 1;
    }

    std::string onnxPath = argv[1];
    std::string outPath  = argv[2];
    std::string password  = argv[3];

    std::cout << "[1/2] Compilation TensorRT...\n";
    auto engine = buildEngine(onnxPath);
    std::cout << "      OK (" << engine.size() << " octets)\n";

    std::cout << "[2/2] Engine encryption...\n";
    auto encrypted = encrypt(engine, password);

    // Write the encrypted engine into the ../models/ directory by default.
    std::ofstream out("../models/" + outPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    std::cout << "       Encrypted engine dumped in : " << outPath << "\n";
    return 0;
}
