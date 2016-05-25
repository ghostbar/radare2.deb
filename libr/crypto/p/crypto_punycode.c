#include <r_util.h>
#include <r_lib.h>
#include <r_crypto.h>

static int flag = 0;

static int punycode_set_key(RCrypto *cry, const ut8 *key, int keylen, int mode, int direction) {
	flag = direction;
	return true;
}

static int punycode_get_key_size(RCrypto *cry) {
	return 0;
}

static bool punycode_use(const char *algo) {
	return !strcmp (algo, "punycode");
}

static int update(RCrypto *cry, const ut8 *buf, int len) {
	char *obuf;
	int olen;
	if (flag) {
		obuf = r_punycode_decode ((const char *)buf, len, &olen);
	} else {
		obuf = r_punycode_encode ((const char *)buf, len, &olen);
	}
	r_crypto_append (cry, (ut8*)obuf, olen);
	free (obuf);
	return 0;
}

static int final(RCrypto *cry, const ut8* buf, int len) {
	return update (cry, buf, len);
}

RCryptoPlugin r_crypto_plugin_punycode = {
	.name = "punycode",
	.set_key = punycode_set_key,
	.get_key_size = punycode_get_key_size,
	.use = punycode_use,
	.update = update,
	.final = final
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_CRYPTO,
	.data = &r_crypto_plugin_punycode,
	.version = R2_VERSION
}
#endif
