// compile_pt.cpp - Encrypt a TorchScript model (.pt) with AES-256-ECB.
//
// This utility encrypts a PyTorch/TorchScript model (.pt file) using AES-256 in ECB mode.
// The encryption key is derived from the password via SHA-256 hashing (consistent with
// all other frameworks in this toolkit).
//
// The encrypted output can later be decrypted by infer_pt.cpp and loaded into torch::jit::load.
//
// Usage: ./compile_pt <password> [input=../models/model.pt] [output=../models/model.pt.enc]
//
// Example:
//   ./compile_pt changeme ../models/model.pt ../models/model.pt.enc

#include <openssl/evp.h>
#include <openssl/sha.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <iostream>
#include <string>

static void derive_key(const std::string& password, unsigned char* key /*32 bytes*/) {
    SHA256(reinterpret_cast<const unsigned char*>(password.data()),
           password.size(), key);
}

static std::vector<unsigned char> read_file(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        std::cerr << "Error: cannot open " << path << std::endl;
        exit(1);
    }
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<unsigned char> buf(size);
    if (!f.read(reinterpret_cast<char*>(buf.data()), size)) {
        std::cerr << "Error: failed to read " << path << std::endl;
        exit(1);
    }
    return buf;
}

static void write_file(const std::string& path, const unsigned char* data, size_t len) {
    std::ofstream f(path, std::ios::binary);
    if (!f) {
        std::cerr << "Error: cannot write " << path << std::endl;
        exit(1);
    }
    f.write(reinterpret_cast<const char*>(data), len);
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " <password> [input=../models/model.pt] [output=../models/model.pt.enc]"
                  << std::endl;
        return 1;
    }

    std::string password    = argv[1];
    std::string input_path  = (argc > 2) ? argv[2] : "../models/model.pt";
    std::string output_path = (argc > 3) ? argv[3] : "../models/model.pt.enc";

    unsigned char key[32];
    derive_key(password, key);

    std::vector<unsigned char> plaintext = read_file(input_path);

    std::vector<unsigned char> ciphertext(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
    int out_len1 = 0, out_len2 = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        std::cerr << "Error: EVP_CIPHER_CTX_new failed" << std::endl;
        return 1;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr) != 1 ||
        EVP_EncryptUpdate(ctx, ciphertext.data(), &out_len1,
                           plaintext.data(), static_cast<int>(plaintext.size())) != 1 ||
        EVP_EncryptFinal_ex(ctx, ciphertext.data() + out_len1, &out_len2) != 1) {
        std::cerr << "Error: encryption failed" << std::endl;
        EVP_CIPHER_CTX_free(ctx);
        return 1;
    }

    EVP_CIPHER_CTX_free(ctx);

    size_t total_len = out_len1 + out_len2;
    write_file(output_path, ciphertext.data(), total_len);

    std::cout << "Encrypted " << input_path << " (" << plaintext.size()
              << " bytes) -> " << output_path << " (" << total_len
              << " bytes)" << std::endl;

    return 0;
}