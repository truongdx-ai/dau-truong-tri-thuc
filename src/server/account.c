#include "account.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>

#define ACCOUNT_ITERATIONS 600000
#define USER_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-"

static void hex_encode(char *out, const unsigned char *bytes, size_t count) {
    const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < count; ++i) {
        out[2 * i] = digits[bytes[i] >> 4];
        out[2 * i + 1] = digits[bytes[i] & 15];
    }
    out[2 * count] = '\0';
}

static AccountResult copy_records(const char *path, FILE *output, const char *username) {
    FILE *input = fopen(path, "r");
    if (input == NULL) {
        return errno == ENOENT ? ACCOUNT_OK : ACCOUNT_ERROR;
    }
    char record[256];
    AccountResult result = ACCOUNT_OK;
    while (fgets(record, sizeof(record), input) != NULL) {
        char *separator = strchr(record, '|');
        if (separator == NULL || strchr(record, '\n') == NULL) {
            result = ACCOUNT_ERROR;
            break;
        }
        if ((size_t)(separator - record) == strlen(username) &&
            strncmp(record, username, strlen(username)) == 0) {
            result = ACCOUNT_EXISTS;
            break;
        }
        if (fputs(record, output) == EOF) {
            result = ACCOUNT_ERROR;
            break;
        }
    }
    if (ferror(input)) {
        result = ACCOUNT_ERROR;
    }
    fclose(input);
    return result;
}

static AccountResult write_record(FILE *output, const char *username, const char *password) {
    unsigned char salt[16], hash[32];
    if (RAND_bytes(salt, sizeof(salt)) != 1 ||
        PKCS5_PBKDF2_HMAC(password, (int)strlen(password), salt, sizeof(salt), ACCOUNT_ITERATIONS,
                          EVP_sha256(), sizeof(hash), hash) != 1) {
        return ACCOUNT_ERROR;
    }
    char salt_hex[sizeof(salt) * 2 + 1], hash_hex[sizeof(hash) * 2 + 1];
    hex_encode(salt_hex, salt, sizeof(salt));
    hex_encode(hash_hex, hash, sizeof(hash));
    return fprintf(output, "%s|pbkdf2-sha256|%d|%s|%s\n", username, ACCOUNT_ITERATIONS, salt_hex,
                   hash_hex) < 0
               ? ACCOUNT_ERROR
               : ACCOUNT_OK;
}

static AccountResult update_db(const char *path, char *temporary, const char *username,
                               const char *password) {
    int fd = mkstemp(temporary);
    if (fd < 0) {
        return ACCOUNT_ERROR;
    }
    FILE *output = fdopen(fd, "w");
    if (output == NULL) {
        close(fd);
        unlink(temporary);
        return ACCOUNT_ERROR;
    }
    AccountResult result = copy_records(path, output, username);
    if (result == ACCOUNT_OK) {
        result = write_record(output, username, password);
    }
    if (result == ACCOUNT_OK && (fflush(output) == EOF || fsync(fd) < 0)) {
        result = ACCOUNT_ERROR;
    }
    if (fclose(output) == EOF) {
        result = ACCOUNT_ERROR;
    }
    if (result == ACCOUNT_OK && rename(temporary, path) == 0) {
        return ACCOUNT_OK;
    }
    unlink(temporary);
    return result == ACCOUNT_OK ? ACCOUNT_ERROR : result;
}

AccountResult account_register(const char *path, const char *username, const char *password) {
    size_t length = strlen(username);
    if (length == 0 || length > 32 || strspn(username, USER_CHARS) != length ||
        password[0] == '\0' || strlen(password) > 128 || strpbrk(password, "|\r\n") != NULL) {
        return ACCOUNT_INVALID;
    }
    char lock_path[PATH_MAX], temporary[PATH_MAX];
    if (snprintf(lock_path, sizeof(lock_path), "%s.lock", path) >= (int)sizeof(lock_path) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >= (int)sizeof(temporary)) {
        return ACCOUNT_ERROR;
    }
    int lock = open(lock_path, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (lock < 0) {
        return ACCOUNT_ERROR;
    }
    AccountResult result = ACCOUNT_ERROR;
    if (flock(lock, LOCK_EX) == 0) {
        result = update_db(path, temporary, username, password);
    }
    close(lock);
    return result;
}
