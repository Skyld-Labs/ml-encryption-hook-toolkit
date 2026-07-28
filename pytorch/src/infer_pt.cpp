// infer_pt.cpp - Victim program: decrypts an encrypted TorchScript model
// and loads it via torch::jit::load(std::istream&).
//
// This is the legitimate (non-hooked) loading path. It reads an encrypted .pt file,
// decrypts the entire buffer in memory (never touching disk with plaintext), and loads
// it into the PyTorch runtime via the public API.
//
// ATTACK SURFACE: torch::jit::load(std::istream&, std::optional<c10::Device>, bool)
//
// This function is a plain exported C++ symbol with a mangled name:
//   _ZN5torch3jit4loadERSiSt8optionalIN3c106DeviceEEb
//
// The hook (pt_load_hook.cpp) intercepts this via symbol interposition by defining
// a function with the exact same signature in the exact same namespace, causing
// the linker to resolve calls to this symbol to the hook first when LD_PRELOAD is used.
//
// Usage: ./infer_pt <model.pt.enc> <password>
// Example: ./infer_pt ../models/model.pt.enc changeme

#include <torch/script.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>

#include <openssl/evp.h>
#include <openssl/sha.h>

static std::vector<char> decrypt(const std::vector<char>& data, const std::string& password) {
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), key);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    std::vector<char> out(data.size() + 32);
    int outLen1 = 0, outLen2 = 0;
    EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(out.data()), &outLen1,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       static_cast<int>(data.size()));
    if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(out.data()) + outLen1, &outLen2) <= 0) {
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
        std::cerr << "Usage: " << argv[0] << " <model.pt.enc> <password>\n";
        return 1;
    }

    std::string encPath  = argv[1];
    std::string password = argv[2];

    std::ifstream file(encPath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Cannot open " << encPath << "\n";
        return 1;
    }
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<char> encBlob(size);
    file.read(encBlob.data(), size);
    std::cout << "Encrypted model: " << size << " bytes\n";

    std::vector<char> decrypted = decrypt(encBlob, password);
    std::cout << "Decrypted model: " << decrypted.size() << " bytes\n";

    // Wrap the decrypted buffer in an in-memory stream - never touches disk.
    std::istringstream stream(std::string(decrypted.begin(), decrypted.end()),
                               std::ios::binary);

    // Hook target: torch::jit::load(istream&, optional<Device>, bool)
    torch::jit::Module module = torch::jit::load(stream);
    module.eval();

    size_t param_count = 0;
    for (const auto& p : module.parameters()) {
        (void)p;
        param_count++;
    }

    std::cout << "Model loaded, parameter tensors: " << param_count << "\n";
    std::cout << "Inference pipeline validated up to model load (hook target confirmed reachable).\n";

    return 0;
}