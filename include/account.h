#ifndef DTTT_ACCOUNT_H
#define DTTT_ACCOUNT_H

typedef enum { ACCOUNT_OK, ACCOUNT_INVALID, ACCOUNT_EXISTS, ACCOUNT_ERROR } AccountResult;

/* Register an ASCII username (1-32 characters) and password (1-128 bytes). */
AccountResult account_register(const char *path, const char *username, const char *password);
AccountResult account_authenticate(const char *path, const char *username, const char *password);

#endif
