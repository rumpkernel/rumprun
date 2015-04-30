#include <sys/types.h>

#include <string.h>

#include <openssl/md5.h>

#include <rumprun/tester.h>

#define TESTSTRING "sum test string this is"

static unsigned char expected[MD5_DIGEST_LENGTH] = {
	0x20, 0x62, 0xda, 0xe2, 0xa3, 0xb4, 0x1d, 0xc7,
	0x0c, 0x23, 0x3f, 0x25, 0xd8, 0xff, 0x5b, 0x6e,
};

int
rumprun_test(int argc, char *argv[])
{
	unsigned char md5sum[MD5_DIGEST_LENGTH];
	MD5_CTX md5ctx;

	MD5_Init(&md5ctx);
	MD5_Update(&md5ctx, TESTSTRING, sizeof(TESTSTRING)-1);
	MD5_Final(md5sum, &md5ctx);

	return memcmp(expected, md5sum, sizeof(expected));
}
