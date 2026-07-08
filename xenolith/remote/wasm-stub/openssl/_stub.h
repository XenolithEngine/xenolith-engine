/* Stub OpenSSL/QUIC/X509 surface for the wasm build. The remote SSL/QUIC transport is native
 * only (the browser does networking); these let the transport code COMPILE and LINK on wasm,
 * where it is never exercised. TODO: replace remote's native transport with a browser
 * transport (Looper pollable handle) and drop this stub. */
#ifndef XL_WASM_OPENSSL_STUB_H
#define XL_WASM_OPENSSL_STUB_H
#include <stddef.h>
#include <stdint.h>
extern "C" {
typedef struct ssl_st SSL;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_method_st SSL_METHOD;
typedef struct bio_st BIO;
typedef struct bio_addrinfo_st BIO_ADDRINFO;
typedef struct bio_addr_st BIO_ADDR;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct evp_md_st EVP_MD;
typedef struct x509_st X509;
typedef struct X509_name_st X509_NAME;
typedef struct asn1_time_st ASN1_TIME;
typedef struct asn1_integer_st ASN1_INTEGER;
typedef struct ssl_conn_close_info_st { int flags; uint64_t error_code; } SSL_CONN_CLOSE_INFO;

/* constants */
#define MBSTRING_ASC 0x1001
enum {
	SSL_ERROR_NONE = 0, SSL_ERROR_WANT_READ = 2, SSL_ERROR_WANT_WRITE = 3,
	SSL_VERIFY_NONE = 0, SSL_TLSEXT_ERR_OK = 0, SSL_TLSEXT_ERR_ALERT_FATAL = 2,
	OPENSSL_NPN_NEGOTIATED = 1, BIO_NOCLOSE = 0, BIO_SOCK_REUSEADDR = 2,
	BIO_LOOKUP_CLIENT = 0, BIO_LOOKUP_SERVER = 1
};

static inline unsigned long ERR_get_error(void) { return 0; }
static inline void ERR_error_string_n(unsigned long, char *buf, size_t len) { if (buf && len) buf[0]=0; }
static inline int ASN1_INTEGER_set(ASN1_INTEGER *, long) { return 0; }
static inline SSL *SSL_accept_connection2(SSL *) { return 0; }
/* SSL_* */
static inline const SSL_METHOD *OSSL_QUIC_client_method(void) { return 0; }
static inline const SSL_METHOD *OSSL_QUIC_server_method(void) { return 0; }
static inline SSL_CTX *SSL_CTX_new(const SSL_METHOD *) { return 0; }
static inline void SSL_CTX_free(SSL_CTX *) {}
static inline void SSL_CTX_set_verify(SSL_CTX *, int, void *) {}
static inline int SSL_CTX_use_certificate(SSL_CTX *, X509 *) { return 0; }
static inline int SSL_CTX_use_PrivateKey(SSL_CTX *, EVP_PKEY *) { return 0; }
static inline void SSL_CTX_set_alpn_select_cb(SSL_CTX *, ...) {}
static inline SSL *SSL_new(SSL_CTX *) { return 0; }
static inline SSL *SSL_new_listener(SSL_CTX *, unsigned) { return 0; }
static inline void SSL_free(SSL *) {}
static inline int SSL_set_fd(SSL *, int) { return 0; }
static inline int SSL_set1_initial_peer_addr(SSL *, const BIO_ADDR *) { return 0; }
static inline int SSL_set_alpn_protos(SSL *, const unsigned char *, unsigned) { return 0; }
static inline int SSL_set_tlsext_host_name(SSL *, const char *) { return 0; }
static inline int SSL_set_blocking_mode(SSL *, int) { return 0; }
static inline int SSL_connect(SSL *) { return 0; }
static inline SSL *SSL_accept_connection(SSL *, unsigned) { return 0; }
static inline SSL *SSL_accept_connection2(SSL *, unsigned) { return 0; }
static inline int SSL_listen(SSL *) { return 0; }
static inline int SSL_read_ex(SSL *, void *, size_t, size_t *r) { if (r) *r = 0; return 0; }
static inline int SSL_write_ex(SSL *, const void *, size_t, size_t *w) { if (w) *w = 0; return 0; }
static inline int SSL_get_error(const SSL *, int) { return SSL_ERROR_NONE; }
static inline int SSL_handle_events(SSL *) { return 0; }
static inline int SSL_get_event_timeout(SSL *, void *, int *inf) { if (inf) *inf = 1; return 0; }
static inline int SSL_shutdown(SSL *) { return 1; }
static inline int SSL_get_conn_close_info(SSL *, SSL_CONN_CLOSE_INFO *, size_t) { return 0; }
static inline int SSL_select_next_proto(unsigned char **, unsigned char *, const unsigned char *, unsigned, const unsigned char *, unsigned) { return 0; }
/* BIO */
static inline int BIO_socket(int, int, int, int) { return -1; }
static inline int BIO_bind(int, const BIO_ADDR *, int) { return 0; }
static inline int BIO_closesocket(int) { return 0; }
static inline int BIO_lookup_ex(const char *, const char *, int, int, int, int, BIO_ADDRINFO **) { return 0; }
static inline void BIO_ADDRINFO_free(BIO_ADDRINFO *) {}
static inline const BIO_ADDR *BIO_ADDRINFO_address(const BIO_ADDRINFO *) { return 0; }
static inline int BIO_ADDRINFO_family(const BIO_ADDRINFO *) { return 0; }
/* EVP */
static inline EVP_PKEY *EVP_EC_gen(const char *) { return 0; }
static inline void EVP_PKEY_free(EVP_PKEY *) {}
static inline const EVP_MD *EVP_sha256(void) { return 0; }
/* X509 */
static inline X509 *X509_new(void) { return 0; }
static inline void X509_free(X509 *) {}
static inline int X509_set_version(X509 *, long) { return 0; }
static inline ASN1_INTEGER *X509_get_serialNumber(X509 *) { return 0; }
static inline X509_NAME *X509_get_subject_name(X509 *) { return 0; }
static inline int X509_set_issuer_name(X509 *, X509_NAME *) { return 0; }
static inline int X509_set_pubkey(X509 *, EVP_PKEY *) { return 0; }
static inline int X509_sign(X509 *, EVP_PKEY *, const EVP_MD *) { return 0; }
static inline ASN1_TIME *X509_getm_notBefore(const X509 *) { return 0; }
static inline ASN1_TIME *X509_getm_notAfter(const X509 *) { return 0; }
static inline ASN1_TIME *X509_gmtime_adj(ASN1_TIME *, long) { return 0; }
static inline int X509_NAME_add_entry_by_txt(X509_NAME *, const char *, int, const unsigned char *, int, int, int) { return 0; }
}
#endif
