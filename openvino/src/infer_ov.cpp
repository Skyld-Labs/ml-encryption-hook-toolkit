// infer_ov.cpp - Load an encrypted OpenVINO IR, decrypt it in memory, run inference.
// This is the attack target: ov::Core::read_model(string, Tensor) receives the decrypted IR.

#include <openvino/openvino.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>

#include <openssl/evp.h>
#include <openssl/sha.h>

static std::vector<unsigned char> decrypt(const std::vector<unsigned char>& data, const std::string& password) {
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), key);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    std::vector<unsigned char> out(data.size() + 32);
    int outLen1 = 0, outLen2 = 0;
    EVP_DecryptUpdate(ctx, out.data(), &outLen1, data.data(), static_cast<int>(data.size()));
    if (EVP_DecryptFinal_ex(ctx, out.data() + outLen1, &outLen2) <= 0) {
        std::cerr << "Decryption failed (wrong key?)\n";
        EVP_CIPHER_CTX_free(ctx);
        exit(1);
    }
    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen1 + outLen2);
    return out;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <model.ov.enc> <password>\n";
        return 1;
    }
    std::string encPath  = argv[1];
    std::string password = argv[2];

    std::ifstream file(encPath, std::ios::binary | std::ios::ate);
    if (!file) { std::cerr << "Cannot open " << encPath << "\n"; return 1; }
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<unsigned char> encBlob(size);
    file.read(reinterpret_cast<char*>(encBlob.data()), size);
    std::cout << "Encrypted IR: " << size << " bytes\n";

    std::vector<unsigned char> packed = decrypt(encBlob, password);
    std::cout << "Decrypted IR: " << packed.size() << " bytes\n";

    uint32_t xmlLen;
    memcpy(&xmlLen, packed.data(), 4);
    std::string xmlStr(reinterpret_cast<char*>(packed.data() + 4), xmlLen);
    unsigned char* weightsPtr = packed.data() + 4 + xmlLen;
    size_t weightsSize = packed.size() - 4 - xmlLen;

    ov::Tensor weightsTensor(ov::element::u8, {weightsSize}, weightsPtr);

    ov::Core core;
    // <-- This exact call is the hook target: read_model(const std::string&, const ov::Tensor&)
    std::shared_ptr<ov::Model> model = core.read_model(xmlStr, weightsTensor);
    std::cout << "Model loaded: " << model->get_friendly_name() << "\n";

    ov::CompiledModel compiled = core.compile_model(model, "CPU");
    ov::InferRequest infer_request = compiled.create_infer_request();
    infer_request.infer();
    std::cout << "Inference OK\n";
    return 0;
}