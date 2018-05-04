#include "utils.h"

Error last_error = Error::NoError;

int32_t base64_decode(char in) {
	// TODO error handling
	if (in == '+') {
		return 62;
	} else if (in == '/') {
		return 63;
	} else if (in >= 'A' && in <= 'Z') {
		return in - 'A';
	} else if (in >= 'a' && in <= 'z') {
		return in - 'a' + ('z' - 'a') + 1;
	} else if (in >= '0' && in <= '9'){
		return in - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
	} else {
		return -1;
	}
}
int32_t vlq_decode(std::string::const_iterator& it) {
	int32_t r = 0;
	uint32_t shift = 0;
	bool hasContinuationBit = false;
	do {
		int32_t i = base64_decode(*it);
		if (i<0) {
			last_error = Error::VlqInvalidBase64;
			return 0;
		}
		hasContinuationBit = i & 32;
		i &= 31;
		r += i << shift;
		shift += 5;
		it++;
	} while (hasContinuationBit);
	bool shouldNegate = r & 1;
	r >>= 1;
	return shouldNegate ? -r : r ;
}
void read_relative_vlq(uint32_t& prev, std::string::const_iterator& it) {
	int32_t decoded = vlq_decode(it);
	int32_t v = decoded + prev;
	// TODO check too big
	if (v < 0)
		last_error = Error::UnexpectedNegativeNumber;
	else
		prev = v;
}
