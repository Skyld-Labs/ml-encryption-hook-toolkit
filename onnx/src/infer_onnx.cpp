// infer_onnx.cpp - Victim program: decrypts an encrypted .onnx model and runs inference.
// ATTACK TARGET: Ort::Session(env, model_data, model_data_length, options)
// Internally calls the C API: OrtApi::CreateSessionFromArray <- hook intercepts this.

#include <onnxruntime_cxx_api.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <openssl/evp.h>
#include <openssl/sha.h>

// Decrypt AES-256-ECB encrypted data using password
static std::vector<unsigned char> decrypt(const std::vector<unsigned char>& data, const std::string& password) {
    // Derive 256-bit key from password using SHA-256
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), key);

    // Setup AES-256-ECB decryption context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    // Decrypt data
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
    // Check arguments: ./infer_onnx <model.onnx.enc> <password>
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <model.onnx.enc> <password>\n";
        return 1;
    }

    std::string encPath  = argv[1];
    std::string password = argv[2];

    // Read encrypted file into memory
    std::ifstream file(encPath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Cannot open " << encPath << "\n";
        return 1;
    }
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<unsigned char> encBlob(size);
    file.read(reinterpret_cast<char*>(encBlob.data()), size);
    std::cout << "Encrypted model: " << size << " bytes\n";

    // Decrypt the model
    std::vector<unsigned char> decrypted = decrypt(encBlob, password);
    std::cout << "Decrypted model: " << decrypted.size() << " bytes\n";

    // Create ONNX Runtime environment
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "infer_onnx");
    Ort::SessionOptions session_options;

    // <-- This call is the hook target: internally invokes 
    // Hook intercepts OrtApi::CreateSessionFromArray (called internally)
    Ort::Session session(env, decrypted.data(), decrypted.size(), session_options);

    std::cout << "Model loaded, input count: " << session.GetInputCount()
              << ", output count: " << session.GetOutputCount() << "\n";
    std::cout << "Inference OK\n";
    return 0;
}