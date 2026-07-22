// infer_tflite.cpp - Load an encrypted .tflite model, decrypt it in memory, run inference.
// This is the attack target: TfLiteModelCreate(model_data, model_size).

#include <tensorflow/lite/c/c_api.h>
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
    // Check arguments: ./infer_tflite <model.tflite.enc> <password>
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <model.tflite.enc> <password>\n";
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

    // ATTACK TARGET: Model is in plaintext here!
    // Hook intercepts TfLiteModelCreate (plain C symbol, no name mangling)
    TfLiteModel* model = TfLiteModelCreate(decrypted.data(), decrypted.size());
    if (!model) {
        std::cerr << "TfLiteModelCreate failed\n";
        return 1;
    }
    std::cout << "Model loaded\n";

    // Setup and run inference
    TfLiteInterpreterOptions* options = TfLiteInterpreterOptionsCreate();
    TfLiteInterpreter* interpreter = TfLiteInterpreterCreate(model, options);
    TfLiteInterpreterAllocateTensors(interpreter);
    TfLiteInterpreterInvoke(interpreter);
    std::cout << "Inference OK\n";

    // Clean up
    TfLiteInterpreterDelete(interpreter);
    TfLiteInterpreterOptionsDelete(options);
    TfLiteModelDelete(model);
    return 0;
}