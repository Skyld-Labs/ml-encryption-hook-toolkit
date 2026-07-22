// compile_onnx.cpp - Encrypt a raw .onnx file (AES-256-ECB).
// Output goes to models/model.onnx.enc

#include <fstream>
#include <iostream>
#include <vector>
#include <openssl/evp.h>
#include <openssl/sha.h>

// Encrypt data using AES-256-ECB with password-derived key
static std::vector<unsigned char> encrypt(const std::vector<unsigned char>& data, const std::string& password) {
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), key);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    std::vector<unsigned char> out(data.size() + 32);
    int outLen1 = 0, outLen2 = 0;
    EVP_EncryptUpdate(ctx, out.data(), &outLen1, data.data(), static_cast<int>(data.size()));
    EVP_EncryptFinal_ex(ctx, out.data() + outLen1, &outLen2);
    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen1 + outLen2);
    return out;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <model.onnx> <password>\n";
        return 1;
    }

    std::string onnxPath  = argv[1];
    std::string password  = argv[2];

    std::ifstream f(onnxPath, std::ios::binary | std::ios::ate);
    if (!f) {
        std::cerr << "Cannot open " << onnxPath << "\n";
        return 1;
    }
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<unsigned char> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    std::cout << "[1/2] Read ONNX model (" << size << " bytes)\n";

    auto encrypted = encrypt(data, password);

    // Écrit dans models/ (au même niveau que src/)
    std::string outPath = "../models/model.onnx.enc";
    std::ofstream out(outPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    std::cout << "[2/2] Encrypted model written to " << outPath << "\n";
    return 0;
}