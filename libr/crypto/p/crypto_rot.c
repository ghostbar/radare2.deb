#include <r_lib.h>
#include <r_crypto.h>

int mod(int a, int b) {
	if (b < 0) {
		return mod (-a, -b);
	}   
	int ret = a % b;
	if (ret < 0) {
		ret += b;
	}   
	return ret;
}

static bool rot_init(ut8 *rotkey, const ut8 *key, int keylen) {
	if (rotkey && key && keylen > 0) {
		int i = atoi ((const char *)key);
		*rotkey = (ut8)mod (i, 26);
		return true;
	}   
	return false;
}

static void rot_crypt(ut8 key, const ut8 *inbuf, ut8 *outbuf, int buflen) {
	int i;
	for (i = 0; i < buflen; i++) {
		outbuf[i] = inbuf[i];
		if ((inbuf[i] < 'a' || inbuf[i] > 'z') && (inbuf[i] < 'A' || inbuf[i] > 'Z')) {
			continue;
		}
		outbuf[i] += key;
		outbuf[i] -= (inbuf[i] >= 'a' && inbuf[i] <= 'z') ? 'a' : 'A';
		outbuf[i] = mod (outbuf[i], 26);
		outbuf[i] += (inbuf[i] >= 'a' && inbuf[i] <= 'z') ? 'a' : 'A';
    }   
}

static ut8 rot_key;

static int rot_set_key(RCrypto *cry, const ut8 *key, int keylen, int mode, int direction) {
	return rot_init (&rot_key, key, keylen);
}

static int rot_get_key_size(RCrypto *cry) {
	//Returning number of bytes occupied by ut8
	return 1;
}

static bool rot_use(const char *algo) {
	return !strcmp (algo, "rot");
}

static int update(RCrypto *cry, const ut8 *buf, int len) {
	ut8 *obuf = calloc (1, len);
	if (!obuf) return false;
	rot_crypt (rot_key, buf, obuf, len);
	r_crypto_append (cry, obuf, len);
	free (obuf);
	return 0;
}

static int final(RCrypto *cry, const ut8 *buf, int len) {
	return update (cry, buf, len);
}

RCryptoPlugin r_crypto_plugin_rot = {
	.name = "rot",
	.set_key = rot_set_key,
	.get_key_size = rot_get_key_size,
	.use = rot_use,
	.update = update,
	.final = final
};

#ifndef CORELIB
struct r_lib_struct_t radare_plugin = {
	.type = R_LIB_TYPE_CRYPTO,
	.data = &r_crypto_plugin_rot,
	.version = R2_VERSION
};
#endif

