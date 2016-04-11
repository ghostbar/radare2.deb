#include <r_lib.h>
#include <r_crypto.h>

#define MAX_ror_KEY_SIZE 32768

struct ror_state {
	ut8 key[MAX_ror_KEY_SIZE];
	int key_size;
};

static bool ror_init(struct ror_state *const state, const ut8 *key, int keylen) {
	if (!state || !key || keylen < 1 || keylen > MAX_ror_KEY_SIZE) {
		return false;
	}
	int i;
	state->key_size = keylen;
	for (i = 0; i < keylen; i++) {
		state->key[i] = key[i];
	}
	return true;
}

static void ror_crypt(struct ror_state *const state, const ut8 *inbuf, ut8 *outbuf, int buflen) {
	int i;
	for (i = 0; i < buflen; i++) {
		outbuf[i] = inbuf[i] >> state->key[i%state->key_size];
	}
}

static struct ror_state st;

static int ror_set_key(RCrypto *cry, const ut8 *key, int keylen, int mode, int direction) {
	return ror_init (&st, key, keylen);
}

static int ror_get_key_size(RCrypto *cry) {
	return st.key_size;
}

static bool ror_use(const char *algo) {
	return !strcmp (algo, "ror");
}

static int update(RCrypto *cry, const ut8 *buf, int len) {
	ut8 *obuf = calloc (1, len);
	if (!obuf) return false;
	ror_crypt (&st, buf, obuf, len);
	r_crypto_append (cry, obuf, len);
	free (obuf);
	return 0;
}

static int final(RCrypto *cry, const ut8 *buf, int len) {
	return update (cry, buf, len);
}

RCryptoPlugin r_crypto_plugin_ror = {
	.name = "ror",
	.set_key = ror_set_key,
	.get_key_size = ror_get_key_size,
	.use = ror_use,
	.update = update,
	.final = final
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_CRYPTO,
	.data = &r_crypto_plugin_ror,
	.version = R2_VERSION
};
#endif
