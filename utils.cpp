#include "utils.h"

Error last_error = Error::NoError;

#ifdef DEBUG
std::ostream& operator<<(std::ostream& os, const indent& ind) {
	for (int i = 0; i < ind.level; i++) {
		os << '\t';
	}
	return os;
}
#endif

class Base64Table {
	char table[256];
public:
	Base64Table() {
		memset(table, -1, 256);
		table['+'] = 62;
		table['/'] = 63;
		for (char i = 'A'; i <= 'Z'; i++) {
			table[i] = i - 'A';
		}
		for (char i = 'a'; i <= 'z'; i++) {
			table[i] = i - 'a' + ('z' - 'a') + 1;
		}
		for (char i = '0'; i <= '9'; i++) {
			table[i] = i - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
		}
	}
	inline char lookup(char in) {
		return table[in];
	}
};
static Base64Table base64_table;

int32_t base64_decode(char in) {
	return base64_table.lookup(in);
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
