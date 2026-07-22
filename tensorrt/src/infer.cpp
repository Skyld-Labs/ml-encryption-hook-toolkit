##include <NvInfer.h>           // TensorRT API (types, runtime, engine, context)
#include <cuda_runtime.h>      // CUDA API for cudaMalloc, streams, etc.
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

#include <openssl/evp.h>       // OpenSSL EVP for encryption/decryption
#include <openssl/sha.h>       // SHA256 for deriving key from password

using namespace nvinfer1;

/* Minimal TensorRT logger — prints WARNING and ERROR messages only */
class Logger : public ILogger {
    void log(Severity severity, const char *msg) noexcept override {
        if (severity <= Severity::kWARNING)
            std::cerr << "[TRT] " << msg << "\n";
    }
} gLogger;


// ─────────────────────────────────────────────
// AES-256-ECB Decryption (symmetric with compile.cpp)
// - Derives a 32-byte key using SHA256(password).
// - AES-256-ECB is simple (note: ECB has known weaknesses).
// ─────────────────────────────────────────────
static std::vector<unsigned char> decrypt(const std::vector<unsigned char>& data,
                                          const std::string& password) {
    unsigned char key[32];
    // Derive 32-byte key from password using SHA256
    SHA256(reinterpret_cast<const unsigned char*>(password.data()),
           password.size(), key);

    // Initialize EVP context for AES-256-ECB decryption
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    std::vector<unsigned char> out(data.size() + 32); // safe output buffer
    int outLen1 = 0, outLen2 = 0;

    // Decrypt the data
    EVP_DecryptUpdate(ctx, out.data(), &outLen1,
                      data.data(), static_cast<int>(data.size()));
    // Finalize and verify padding validity; if fails => incorrect key
    if (EVP_DecryptFinal_ex(ctx, out.data() + outLen1, &outLen2) <= 0) {
        std::cerr << "Decryption failed (wrong key?)\n";
        EVP_CIPHER_CTX_free(ctx);
        exit(1);
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen1 + outLen2); // actual size
    return out;
}


int main(int argc, char **argv)
{
    // Argument validation
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <model.engine.enc> <password>\n";
        return 1;
    }
    const char *engine_path = argv[1];
    std::string password    = argv[2];

    // Read the encrypted engine from file
    std::ifstream file(engine_path, std::ios::binary | std::ios::ate);
    if (!file) { std::cerr << "Cannot open " << engine_path << "\n"; return 1; }
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<unsigned char> encBlob(size);
    file.read(reinterpret_cast<char*>(encBlob.data()), size);
    file.close();
    std::cout << " Engine: " << engine_path << " (" << size << " bytes)\n";

    // Decrypt in memory (avoid writing decrypted engine to disk)
    std::vector<unsigned char> blob = decrypt(encBlob, password);
    std::cout << "Decrypted engine: " << blob.size() << " bytes\n";

    // Create TensorRT runtime and deserialize engine from memory buffer
    IRuntime *runtime = createInferRuntime(gLogger);
    ICudaEngine *engine = runtime->deserializeCudaEngine(blob.data(), blob.size());
    if (!engine) {
        std::cerr << "deserializeCudaEngine failed\n";
        return 1;
    }

    // Create an execution context
    IExecutionContext *ctx = engine->createExecutionContext();

    // Get number of input/output tensors
    int nb = engine->getNbIOTensors();
    std::cout << "IO tensors: " << nb << "\n";

    /* Display dims of each tensor and handle dynamic dimensions
       - If a dimension is negative (dynamic), replace with 1 here.
       - Then call setInputShape to apply the shape to the context. */
    for (int i = 0; i < nb; i++) {
        const char *name = engine->getIOTensorName(i);
        Dims dims = engine->getTensorShape(name);
        std::cout << "  [" << i << "] " << name << " dims:";
        for (int d = 0; d < dims.nbDims; d++) {
            std::cout << " " << dims.d[d];
        }
        std::cout << "\n";

        if (engine->getTensorIOMode(name) == TensorIOMode::kINPUT) {
            bool has_dynamic = false;
            for (int d = 0; d < dims.nbDims; d++)
                if (dims.d[d] < 0) {
                    // Simple strategy: replace dynamic dimension with 1
                    dims.d[d] = 1;
                    has_dynamic = true;
                }
            if (has_dynamic) {
                // Apply input shape to the context
                ctx->setInputShape(name, dims);
            }
        }
    }

    /* Allocate GPU buffers for each I/O tensor (float assumed)
       - Calculate number of elements n = product(dims)
       - Use cudaMalloc + cudaMemset
       - Bind each buffer to tensor name via setTensorAddress */
    std::vector<void *> buffers(nb, nullptr);
    for (int i = 0; i < nb; i++) {
        const char *name = engine->getIOTensorName(i);
        Dims dims = ctx->getTensorShape(name); // effective shape in context
        size_t n = 1;
        for (int d = 0; d < dims.nbDims; d++) {
            n *= (dims.d[d] > 0 ? dims.d[d] : 1);
        }
        // Assuming float here; adapt if needed (e.g., int8)
        cudaMalloc(&buffers[i], n * sizeof(float));
        cudaMemset(buffers[i], 0, n * sizeof(float));
        ctx->setTensorAddress(name, buffers[i]);
    }

    // Run inference on a CUDA stream
    cudaStream_t stream;
    cudaStreamCreate(&stream);
    ctx->enqueueV3(stream);               // submit inference (non-blocking)
    cudaStreamSynchronize(stream);        // wait for completion
    cudaStreamDestroy(stream);
    std::cout << "Inference OK\n";

    // Free GPU and TensorRT resources
    for (void *b : buffers) cudaFree(b);
    delete ctx;
    delete engine;
    delete runtime;
    return 0;
}