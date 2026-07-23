// infer_onnx_c.c - Victim program (pure C) - decrypts and runs inference
// via the ONNX Runtime C API directly (no C++ wrapper at all).
//
// This demonstrates that onnx_session_hook.c works regardless of whether
// the victim application is written in C or C++: both paths ultimately
// call OrtApi::CreateSessionFromArray.

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <onnxruntime_c_api.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

static const OrtApi* g_ort = NULL;

#define CHECK_ORT_STATUS(status)                                   \
    do {                                                           \
        OrtStatus* _s = (status);                                  \
        if (_s != NULL) {                                          \
            fprintf(stderr, "[ORT ERROR] %s\n",                    \
                    g_ort->GetErrorMessage(_s));                   \
            g_ort->ReleaseStatus(_s);                               \
            exit(1);                                               \
        }                                                          \
    } while (0)

static unsigned char* decrypt_buffer(const unsigned char* data,
                                      size_t data_len,
                                      const char* password,
                                      size_t* out_len)
{
    unsigned char key[32];
    SHA256((const unsigned char*)password, strlen(password), key);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ecb(), NULL, key, NULL);

    unsigned char* out = malloc(data_len + 32);
    int outLen1 = 0, outLen2 = 0;

    EVP_DecryptUpdate(ctx, out, &outLen1, data, (int)data_len);

    if (EVP_DecryptFinal_ex(ctx, out + outLen1, &outLen2) <= 0) {
        fprintf(stderr, "Decryption failed (wrong password?)\n");
        EVP_CIPHER_CTX_free(ctx);
        free(out);
        exit(1);
    }

    EVP_CIPHER_CTX_free(ctx);
    *out_len = (size_t)(outLen1 + outLen2);
    return out;
}

int main(int argc, char** argv)
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <model.onnx.enc> <password>\n", argv[0]);
        return 1;
    }

    const char* enc_path = argv[1];
    const char* password = argv[2];

    FILE* f = fopen(enc_path, "rb");
    if (!f) { perror("fopen"); return 1; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char* enc_data = malloc((size_t)size);
    fread(enc_data, 1, (size_t)size, f);
    fclose(f);
    printf("Encrypted model: %ld bytes\n", size);

    size_t dec_len = 0;
    unsigned char* dec_data =
        decrypt_buffer(enc_data, (size_t)size, password, &dec_len);
    printf("Decrypted model: %zu bytes\n", dec_len);
    free(enc_data);

    // Pure C API entry point - no C++ involved anywhere below this line.
    g_ort = OrtGetApiBase()->GetApi(ORT_API_VERSION);
    if (!g_ort) {
        fprintf(stderr, "Failed to get ONNX Runtime C API\n");
        return 1;
    }

    OrtEnv* env;
    CHECK_ORT_STATUS(g_ort->CreateEnv(ORT_LOGGING_LEVEL_WARNING,
                                       "infer_onnx_c", &env));

    OrtSessionOptions* session_options;
    CHECK_ORT_STATUS(g_ort->CreateSessionOptions(&session_options));

    OrtSession* session;
    // Hook target: OrtApi::CreateSessionFromArray
    CHECK_ORT_STATUS(g_ort->CreateSessionFromArray(
        env, dec_data, dec_len, session_options, &session));

    size_t input_count = 0, output_count = 0;
    CHECK_ORT_STATUS(g_ort->SessionGetInputCount(session, &input_count));
    CHECK_ORT_STATUS(g_ort->SessionGetOutputCount(session, &output_count));

    printf("Model loaded, input count: %zu, output count: %zu\n",
           input_count, output_count);
    printf("Inference OK\n");

    free(dec_data);
    g_ort->ReleaseSessionOptions(session_options);
    g_ort->ReleaseSession(session);
    g_ort->ReleaseEnv(env);

    return 0;
}