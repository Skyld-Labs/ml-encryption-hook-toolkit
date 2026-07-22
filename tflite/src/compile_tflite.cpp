/// compile_tflite.cpp - Encrypt a raw .tflite file (AES-256-ECB).
// TFLite loads FlatBuffer bytes directly, no IR conversion needed.

#include <fstream>
#include <iostream>
#include <vector>

#include <openssl/evp.h>
#include <openssl/sha.h>

// Encrypt data using AES-256-ECB with password-derived key
static std::vector<unsigned char> encrypt(const std::vector<unsigned char>& data, const std::string& password) {
    // Derive 256-bit key from password using SHA-256
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), key);

    // Setup AES-256-ECB encryption context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    // Encrypt data
    std::vector<unsigned char> out(data.size() + 32);
    int outLen1 = 0, outLen2 = 0;
    EVP_EncryptUpdate(ctx, out.data(), &outLen1, data.data(), static_cast<int>(data.size()));
    EVP_EncryptFinal_ex(ctx, out.data() + outLen1, &outLen2);
    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen1 + outLen2);
    return out;
}

int main(int argc, char** argv) {
    // Check arguments: ./compile_tflite <model.tflite> <output.tflite.enc> <password>
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <model.tflite> <output.tflite.enc> <password>\n";
        return 1;
    }

    std::string modelPath = argv[1];
    std::string outPath   = argv[2];
    std::string password  = argv[3];

    // Read the entire .tflite file into memory
    std::ifstream f(modelPath, std::ios::binary | std::ios::ate);
    if (!f) {
        std::cerr << "Cannot open " << modelPath << "\n";
        return 1;
    }
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<unsigned char> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    std::cout << "[1/2] Read TFLite model (" << size << " bytes)\n";

    // Encrypt the model
    auto encrypted = encrypt(data, password);

    // Write encrypted model to runtime folder
    std::ofstream out("../models/" + outPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    std::cout << "[2/2] Encrypted model written to ../models/" << outPath << "\n";
    return 0;
}