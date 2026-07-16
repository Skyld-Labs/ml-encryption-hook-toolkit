#include <NvInfer.h>           // API TensorRT (types, runtime, engine, context)
#include <cuda_runtime.h>      // API CUDA pour cudaMalloc, streams, etc.
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

#include <openssl/evp.h>       // OpenSSL EVP pour chiffrement/déchiffrement
#include <openssl/sha.h>       // SHA256 pour dériver la clé depuis le mot de passe

using namespace nvinfer1;

/* Logger TensorRT minimal — affiche les messages WARNING et erreurs */
class Logger : public ILogger {
    void log(Severity severity, const char *msg) noexcept override {
        if (severity <= Severity::kWARNING)
            std::cerr << "[TRT] " << msg << "\n";
    }
} gLogger;


// ─────────────────────────────────────────────
// Déchiffrement AES-256-ECB (symétrique avec compile.cpp)
// - On dérive une clé 32 octets par SHA256(password).
// - AES-256-ECB est simple (attention aux faiblesses d'ECB).
// ─────────────────────────────────────────────
static std::vector<unsigned char> decrypt(const std::vector<unsigned char>& data,
                                          const std::string& password) {
    unsigned char key[32];
    // Dérive la clé (32 octets) depuis le mot de passe via SHA256
    SHA256(reinterpret_cast<const unsigned char*>(password.data()),
           password.size(), key);

    // Initialise le contexte EVP pour décryptage AES-256-ECB
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), nullptr, key, nullptr);

    std::vector<unsigned char> out(data.size() + 32); // tampon de sortie (sûr)
    int outLen1 = 0, outLen2 = 0;

    // Décrypte les données
    EVP_DecryptUpdate(ctx, out.data(), &outLen1,
                      data.data(), static_cast<int>(data.size()));
    // Finalise et vérifie la validité du padding ; si fail => clé incorrecte
    if (EVP_DecryptFinal_ex(ctx, out.data() + outLen1, &outLen2) <= 0) {
        std::cerr << "Échec du déchiffrement (mauvaise clé ?)\n";
        EVP_CIPHER_CTX_free(ctx);
        exit(1);
    }

    EVP_CIPHER_CTX_free(ctx);
    out.resize(outLen1 + outLen2); // taille réelle
    return out;
}


int main(int argc, char **argv)
{
    // Vérification des arguments
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <model.engine.enc> <clé>\n";
        return 1;
    }
    const char *engine_path = argv[1];
    std::string password    = argv[2];

    // Lit l'engine chiffré depuis le fichier
    std::ifstream file(engine_path, std::ios::binary | std::ios::ate);
    if (!file) { std::cerr << "Cannot open " << engine_path << "\n"; return 1; }
    size_t size = file.tellg();
    file.seekg(0);
    std::vector<unsigned char> encBlob(size);
    file.read(reinterpret_cast<char*>(encBlob.data()), size);
    file.close();
    std::cout << " Engine: " << engine_path << " (" << size << " bytes)\n";

    // Déchiffre en mémoire (on évite d'écrire l'engine déchiffré sur disque)
    std::vector<unsigned char> blob = decrypt(encBlob, password);
    std::cout << "Decrypted engine: " << blob.size() << " bytes\n";

    // Crée le runtime TensorRT et désérialise l'engine depuis le tampon mémoire
    IRuntime *runtime = createInferRuntime(gLogger);
    ICudaEngine *engine = runtime->deserializeCudaEngine(blob.data(), blob.size());
    if (!engine) {
        std::cerr << "deserializeCudaEngine failed\n";
        return 1;
    }

    // Crée un contexte d'exécution
    IExecutionContext *ctx = engine->createExecutionContext();

    // Récupère le nombre de tenseurs d'entrée/sortie (I/O)
    int nb = engine->getNbIOTensors();
    std::cout << "IO tensors: " << nb << "\n";

    /* Affiche les dims de chaque tenseur et fixe les dims dynamiques
       - Si une dimension est négative (dynamiques), on la remplace par 1 ici.
       - Ensuite on appelle setInputShape pour appliquer la forme au contexte. */
    for (int i = 0; i < nb; i++) {
        const char *name = engine->getIOTensorName(i);
        Dims dims = engine->getTensorShape(name);
        std::cout << "  [" << i << "] " << name << " dims:";
        for (int d = 0; d < dims.nbDims; d++) {
            std::cout << " " << dims.d[d];
        }
        std::cout << "\n";

        if (engine->getTensorIOMode(name) == TensorIOMode::kINPUT) {
            bool has_dynamic = false;
            for (int d = 0; d < dims.nbDims; d++)
                if (dims.d[d] < 0) {
                    // Simple stratégie : remplacer dimension dynamique par 1
                    dims.d[d] = 1;
                    has_dynamic = true;
                }
            if (has_dynamic) {
                // Applique la forme d'entrée au contexte
                ctx->setInputShape(name, dims);
            }
        }
    }

    /* Alloue les buffers GPU pour chaque tenseur I/O (float assumed)
       - calcule le nombre d'éléments n = produit(dims)
       - utilise cudaMalloc + cudaMemset
       - lie chaque buffer au nom du tenseur via setTensorAddress */
    std::vector<void *> buffers(nb, nullptr);
    for (int i = 0; i < nb; i++) {
        const char *name = engine->getIOTensorName(i);
        Dims dims = ctx->getTensorShape(name); // forme effective dans le contexte
        size_t n = 1;
        for (int d = 0; d < dims.nbDims; d++) {
            n *= (dims.d[d] > 0 ? dims.d[d] : 1);
        }
        // Ici on suppose float ; adapte si besoin (ex : int8)
        cudaMalloc(&buffers[i], n * sizeof(float));
        cudaMemset(buffers[i], 0, n * sizeof(float));
        ctx->setTensorAddress(name, buffers[i]);
    }

    // Lance l'inférence sur un stream CUDA
    cudaStream_t stream;
    cudaStreamCreate(&stream);
    ctx->enqueueV3(stream);               // submit inference (non-blocking)
    cudaStreamSynchronize(stream);        // attend la fin
    cudaStreamDestroy(stream);
    std::cout << "Inference OK\n";

    // Libère les ressources GPU et TensorRT
    for (void *b : buffers) cudaFree(b);
    delete ctx;
    delete engine;
    delete runtime;
    return 0;
}