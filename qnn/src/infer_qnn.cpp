// infer_qnn.cpp - Victim program: decrypts an encrypted QNN context binary
// and loads it via QnnContext_createFromBinary().
// Hook target: QnnContext_createFromBinary()

#include <QNN/QnnInterface.h>
#include <QNN/QnnContext.h>
#include <QNN/QnnBackend.h>

#include <dlfcn.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <string> 

#include <openssl/evp.h>
#include <openssl/sha.h>

static std::vector<unsigned char> decrypt(const std::vector<unsigned char>& data,
                                           const std::string& password) {
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

// Resolves the QNN_INTERFACE function table from the loaded backend library.
// Follows the standard pattern from QNN sample apps (QnnInterface_getProviders).
static const QnnInterface_t* getQnnInterface(void* backendLibHandle) {
    typedef Qnn_ErrorHandle_t (*GetProvidersFn)(const QnnInterface_t***, uint32_t*);

    auto getProviders = reinterpret_cast<GetProvidersFn>(
        dlsym(backendLibHandle, "QnnInterface_getProviders"));
    if (!getProviders) {
        std::cerr << "dlsym(QnnInterface_getProviders) failed: " << dlerror() << "\n";
        exit(1);
    }

    const QnnInterface_t** providerList = nullptr;
    uint32_t numProviders = 0;
    if (getProviders(&providerList, &numProviders) != QNN_SUCCESS || numProviders == 0) {
        std::cerr << "QnnInterface_getProviders returned no providers\n";
        exit(1);
    }

    // Take the first available provider (only one backend loaded here).
    return providerList[0];
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0]
                  << " <model_context.bin.enc> <password> <path/to/libQnnGpu.so>\n";
        return 1;
    }

    std::string encPath   = argv[1];
    std::string password  = argv[2];
    std::string backendSo = argv[3];

    // --- 1. Read + decrypt the context binary ---
    std::ifstream file(encPath, std::ios::binary | std::ios::ate);
    if (!file) {
        std::cerr << "Cannot open " << encPath << "\n";
        return 1;
    }
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<unsigned char> encBlob(size);
    file.read(reinterpret_cast<char*>(encBlob.data()), size);
    std::cout << "Encrypted context binary: " << size << " bytes\n";

    std::vector<unsigned char> decrypted = decrypt(encBlob, password);
    std::cout << "Decrypted context binary: " << decrypted.size() << " bytes\n";

    // --- 2. Load the QNN backend (GPU, the only x86-compatible backend
    //         that supports context caching) ---
    void* backendLibHandle = dlopen(backendSo.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!backendLibHandle) {
        std::cerr << "dlopen(" << backendSo << ") failed: " << dlerror() << "\n";
        return 1;
    }

    const QnnInterface_t* qnnInterface = getQnnInterface(backendLibHandle);
    std::cout << "QNN backend interface acquired (version "
              << qnnInterface->apiVersion.coreApiVersion.major << "."
              << qnnInterface->apiVersion.coreApiVersion.minor << ")\n";

    // --- 3. Create the backend handle ---
    Qnn_BackendHandle_t backendHandle = nullptr;
    Qnn_ErrorHandle_t err = qnnInterface->QNN_INTERFACE_VER_NAME.backendCreate(
        nullptr /* logger */, nullptr /* config */, &backendHandle);
    if (err != QNN_SUCCESS) {
        std::cerr << "QnnBackend_create failed, error=" << err << "\n";
        return 1;
    }
    std::cout << "Backend created\n";

    // --- 4. Hook target: QnnContext_createFromBinary ---
    // The 'decrypted' buffer only ever exists in RAM, never written back
    // to disk in plaintext.
    Qnn_ContextHandle_t contextHandle = nullptr;
    err = qnnInterface->QNN_INTERFACE_VER_NAME.contextCreateFromBinary(
        backendHandle,
        nullptr /* device */,
        nullptr /* config */,
        decrypted.data(),
        static_cast<Qnn_ContextBinarySize_t>(decrypted.size()),
        &contextHandle,
        nullptr /* profile */);

    if (err != QNN_SUCCESS) {
        std::cerr << "QnnContext_createFromBinary failed, error=" << err << "\n";
        qnnInterface->QNN_INTERFACE_VER_NAME.backendFree(backendHandle);
        return 1;
    }

    std::cout << "Context created successfully from decrypted binary!\n";
    std::cout << "Inference pipeline validated up to context load (hook target confirmed reachable).\n";

    // --- 5. Cleanup ---
    qnnInterface->QNN_INTERFACE_VER_NAME.contextFree(contextHandle, nullptr);
    qnnInterface->QNN_INTERFACE_VER_NAME.backendFree(backendHandle);
    dlclose(backendLibHandle);

    return 0;
}