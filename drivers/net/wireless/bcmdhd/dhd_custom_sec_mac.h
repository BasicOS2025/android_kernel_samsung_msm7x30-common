/* SPDX-License-Identifier: GPL-2.0 */
#ifndef DHD_CUSTOM_SEC_MAC_H
#define DHD_CUSTOM_SEC_MAC_H

static inline int dhd_mac_hex(unsigned char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

/* Parse a MAC file without wide sscanf stores into six one-byte octets. */
static inline int dhd_parse_mac(const char *text, unsigned int length,
				unsigned char address[6])
{
	unsigned char parsed[6];
	unsigned int i, nonzero = 0;
	int high, low;

	if (length != 17 && length != 18)
		return -1;
	if (length == 18 && text[17] != '\n' && text[17] != '\0')
		return -1;
	for (i = 0; i < 6; i++) {
		high = dhd_mac_hex(text[i * 3]);
		low = dhd_mac_hex(text[i * 3 + 1]);
		if (high < 0 || low < 0 || (i < 5 && text[i * 3 + 2] != ':'))
			return -1;
		parsed[i] = (high << 4) | low;
		nonzero |= parsed[i];
	}
	if (!nonzero || (parsed[0] & 1))
		return -1;
	for (i = 0; i < 6; i++)
		address[i] = parsed[i];
	return 0;
}

#endif
