/* SPDX-License-Identifier: GPL-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../drivers/net/wireless/bcmdhd/dhd_custom_sec_mac.h"

static void check(const char *input, int valid, const unsigned char *expected)
{
	struct {
		unsigned char before;
		unsigned char mac[6];
		unsigned char after;
	} guarded = { 0xa5, { 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc }, 0x5a };
	unsigned char unchanged[6];
	int result;
	memcpy(unchanged, guarded.mac, sizeof(unchanged));
	result = dhd_parse_mac(input, strlen(input), guarded.mac);
	assert((result == 0) == valid);
	assert(guarded.before == 0xa5 && guarded.after == 0x5a);
	assert(!memcmp(guarded.mac, valid ? expected : unchanged, sizeof(unchanged)));
}

int main(void)
{
	const unsigned char original[6] = { 0x00, 0x12, 0x34, 0xff, 0x80, 0x00 };
	const unsigned char local[6] = { 0x02, 0xff, 0xff, 0xff, 0xff, 0xff };
	unsigned int i, j, state = 0x12345678;
	check("00:12:34:ff:80:00", 1, original);
	check("00:12:34:FF:80:00\n", 1, original);
	check("02:ff:ff:ff:ff:ff", 1, local);
	check("", 0, NULL);
	check("00:12:34:ff:80", 0, NULL);
	check("00-12-34-ff-80-00", 0, NULL);
	check("00:12:34:gg:80:00", 0, NULL);
	check("00:00:00:00:00:00", 0, NULL);
	check("01:12:34:ff:80:00", 0, NULL);
	check("ff:ff:ff:ff:ff:ff", 0, NULL);
	check("00:12:34:ff:80:00X", 0, NULL);
	check("00:12:34:ff:80:00\nextra", 0, NULL);
	for (i = 0; i < 100000; i++) {
		char input[32];
		unsigned char output[8] = { 0xa5, 0, 0, 0, 0, 0, 0, 0x5a };
		unsigned int length = i % sizeof(input);
		for (j = 0; j < sizeof(input); j++) {
			state = state * 1664525u + 1013904223u;
			input[j] = (char)(state >> 24);
		}
		dhd_parse_mac(input, length, &output[1]);
		assert(output[0] == 0xa5 && output[7] == 0x5a);
	}
	puts("MAC parsing: regression cases and 100000 bounded inputs passed");
	return 0;
}
