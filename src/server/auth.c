#include "account.h"
#include "protocol.h"

#include <errno.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int decode(const char *text, unsigned char *bytes, size_t count) {
    const char digits[] = "0123456789abcdef";
    if (strlen(text) != count * 2) {
        return -1;
    }
    for (size_t i = 0; i < count; ++i) {
        const char *high = strchr(digits, text[2 * i]);
        const char *low = strchr(digits, text[2 * i + 1]);
        if (high == NULL || low == NULL) {
            return -1;
        }
        bytes[i] = (unsigned char)((high - digits) * 16 + (low - digits));
    }
    return 0;
}

static AccountResult verify_record(char *line, const char *password) {
    ProtocolMessage record;
    if (protocol_parse(line, &record) < 0 || record.count != 5 ||
        strcmp(record.fields[1], "pbkdf2-sha256") != 0) {
        return ACCOUNT_ERROR;
    }
    char *end;
    long iterations = strtol(record.fields[2], &end, 10);
    unsigned char salt[16], expected[32], actual[32];
    if (*end != '\0' || iterations < 1 || iterations > 1000000 ||
        decode(record.fields[3], salt, sizeof(salt)) < 0 ||
        decode(record.fields[4], expected, sizeof(expected)) < 0) {
        return ACCOUNT_ERROR;
    }
    if (PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, sizeof(salt), (int)iterations,
                          EVP_sha256(), sizeof(actual), actual) != 1) {
        return ACCOUNT_ERROR;
    }
    int matches = CRYPTO_memcmp(expected, actual, sizeof(actual)) == 0;
    OPENSSL_cleanse(actual, sizeof(actual));
    return matches ? ACCOUNT_OK : ACCOUNT_INVALID;
}

AccountResult account_authenticate(const char *path, const char *username, const char *password) {
    if (strlen(username) > 32 || strlen(password) > 128 || username[0] == '\0') {
        return ACCOUNT_INVALID;
    }
    FILE *input = fopen(path, "r");
    if (input == NULL) {
        return errno == ENOENT ? ACCOUNT_INVALID : ACCOUNT_ERROR;
    }
    AccountResult result = ACCOUNT_INVALID;
    char line[256];
    while (fgets(line, sizeof(line), input) != NULL) {
        char *newline = strchr(line, '\n');
        char *separator = strchr(line, '|');
        if (newline == NULL || separator == NULL) {
            result = ACCOUNT_ERROR;
            break;
        }
        *newline = '\0';
        if ((size_t)(separator - line) == strlen(username) &&
            strncmp(line, username, strlen(username)) == 0) {
            result = verify_record(line, password);
            break;
        }
    }
    if (ferror(input)) {
        result = ACCOUNT_ERROR;
    }
    fclose(input);
    return result;
}
