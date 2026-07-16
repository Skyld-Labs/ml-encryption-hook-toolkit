#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <string>
#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

class TRTLogger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* msg) noexcept override {
        if (severity <= Severity::kWARNING)
            std::cerr << "[TRT] " << msg << "\n";
    }
};

static std::vector<unsigned char> buildEngine(const std::string& onnxPath) {
    TRTLogger logger;

    auto builder = std::unique_ptr<nvinfer1::IBuilder>(
        nvinfer1::createInferBuilder(logger));
    nvinfer1::INetworkDefinition* rawNetwork = builder->createNetworkV2(0);
    auto parser = std::unique_ptr<nvonnxparser::IParser>(
        nvonnxparser::createParser(*rawNetwork, logger));
    std::ifstream f(onnxPath, std::ios::binary | std::ios::ate);
    if (!f)
    {
        std::cerr << "Impossible to open : " << onnxPath << "\n"; exit(1);
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<char> onnxData(size);
    f.read(onnxData.data(), size);
    if (!parser->parse(onnxData.data(), onnxData.size())) {
        std::cerr << "Parsing error\n";
        exit(1);
    }

    auto config = std::unique_ptr<nvinfer1::IBuilderConfig>(
        builder->createBuilderConfig());
    config->setProfilingVerbosity(nvinfer1::ProfilingVerbosity::kDETAILED);

    auto serialized = std::unique_ptr<nvinfer1::IHostMemory>(
        builder->buildSerializedNetwork(*rawNetwork, *config));
    if (!serialized)
    {
        std::cerr << "Build error\n"; exit(1);
    }

    const auto* ptr = static_cast<const unsigned char*>(serialized->data());
    std::vector<unsigned char> result(ptr, ptr + serialized->size());

    serialized.reset();
    config.reset();
    parser.reset();
    delete rawNetwork;
    builder.reset();

    return result;
}

static std::vector<unsigned char> encrypt(const std::vector<unsigned char>& data,
const std::string& password) {
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char*>(password.data()),
    password.size(), key);
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);
    std::vector<unsigned char> out(data.size() + 32);
    int outLen1 = 0, outLen2 = 0;
    EVP_EncryptUpdate(ctx, out.data(), &outLen1,
    data.data(), static_cast<int>(data.size()));
    EVP_EncryptFinal_ex(ctx, out.data() + outLen1, &outLen2);
    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen1 + outLen2);
    return out;
}

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
    std::ofstream out("../runtime/" + outPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    std::cout << "       Encrypted engine dumped in : " << outPath << "\n";
    return 0;
}
