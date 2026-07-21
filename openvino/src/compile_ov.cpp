// compile_ov.cpp - Convert an ONNX model to OpenVINO IR (XML + weights) and encrypt the result.
// Packed format: [4-byte little-endian XML length][XML bytes][weights bytes]

#include <openvino/openvino.hpp>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdio>

#include <openssl/evp.h>
#include <openssl/sha.h>

static std::vector<unsigned char> encrypt(const std::vector<unsigned char>& data, const std::string& password) {
    // Derive a 256-bit key from the password with SHA-256.
    unsigned char key[32];
    SHA256(reinterpret_cast<const unsigned char*>(password.data()), password.size(), key);

    // Encrypt the payload with AES-256 in ECB mode.
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

static std::vector<unsigned char> readFile(const std::string& path) {
    // Read the full file into memory so it can be packed and encrypted.
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) { std::cerr << "Cannot open " << path << "\n"; exit(1); }
    size_t size = f.tellg();
    f.seekg(0);
    std::vector<unsigned char> data(size);
    f.read(reinterpret_cast<char*>(data.data()), size);
    return data;
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <model.onnx> <output.ov.enc> <password>\n";
        return 1;
    }
    std::string onnxPath = argv[1];
    std::string outPath  = argv[2];
    std::string password = argv[3];

    // Load the ONNX model and convert it to OpenVINO's native IR format (XML graph + binary weights).
    ov::Core core;
    std::shared_ptr<ov::Model> model = core.read_model(onnxPath);

    // OpenVINO serialization writes a temporary XML file and a temporary BIN file.
    const std::string xmlTmp = "model_tmp.xml";
    const std::string binTmp = "model_tmp.bin";
    ov::serialize(model, xmlTmp, binTmp);
    std::cout << "[1/3] Converted ONNX to OpenVINO IR\n";

    // Load both IR artifacts so they can be stored in a single encrypted blob.
    std::vector<unsigned char> xmlData = readFile(xmlTmp);
    std::vector<unsigned char> weightsData = readFile(binTmp);
    std::cout << "      IR XML: " << xmlData.size() << " bytes, weights: " << weightsData.size() << " bytes\n";

    // Pack the XML and weights into a single buffer so only one file needs to be encrypted and transported.
    std::vector<unsigned char> packed;
    // Prefix the XML length so the runtime can split XML and weights after decryption.
    uint32_t xmlLen = static_cast<uint32_t>(xmlData.size());
    packed.resize(4);
    memcpy(packed.data(), &xmlLen, 4);
    packed.insert(packed.end(), xmlData.begin(), xmlData.end());
    packed.insert(packed.end(), weightsData.begin(), weightsData.end());
    std::cout << "[2/3] Packed IR into a single buffer (" << packed.size() << " bytes)\n";

    auto encrypted = encrypt(packed, password);
    // Write the encrypted payload to the runtime directory used by the loader.
    std::ofstream out("../models/" + outPath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    std::cout << "[3/3] Encrypted IR written to ../models/" << outPath << "\n";

    // Clean up the temporary IR files created during serialization.
    std::remove(xmlTmp.c_str());
    std::remove(binTmp.c_str());
    return 0;
}