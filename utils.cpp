#include "utils.h"

#ifdef DEBUG
std::ostream& operator<<(std::ostream& os, const indent& ind) {
	for (int i = 0; i < ind.level; i++) {
		os << '\t';
	}
	return os;
}
#endif

[[cheerp::genericjs]]
[[noreturn]]
void throw_error(Error e) {
	client::String* msg = new client::String("Error parsing mappings (code ");
	msg = msg->concat(e)->concat("): ");
	switch (e) {
	case Error::UnexpectedNegativeNumber:
		msg = msg->concat("the mappings contained a negative line, column, source index, or name index");
		break;
	case Error::UnexpectedlyBigNumber:
		msg = msg->concat("the mappings contained a number larger than 2**32");
		break;
	case Error::VlqUnexpectedEof:
		msg = msg->concat("reached EOF while in the middle of parsing a VLQ");
		break;
	case Error::VlqInvalidBase64:
		msg = msg->concat("invalid base 64 character while parsing a VLQ");
		break;
	case Error::VlqOverflow:
		msg = msg->concat("the number parsed from the VLQ does not fit in a 64 bit integer");
		break;
	case Error::NoError:
		msg = msg->concat("No error. This is a bug");
		break;
	}
	__asm__("throw new Error(%0)" : : "r"(msg));
	// This is to shut down the noreturn warning
	while(true){}
}

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
std::pair<int32_t, Error> vlq_decode(std::string::const_iterator& it) {
	int32_t r = 0;
	uint32_t shift = 0;
	bool hasContinuationBit = false;
	do {
		int32_t i = base64_decode(*it);
		if (i<0) {
			return std::make_pair(i, Error::VlqInvalidBase64);
		}
		hasContinuationBit = i & 32;
		i &= 31;
		r += i << shift;
		shift += 5;
		it++;
	} while (hasContinuationBit);
	bool shouldNegate = r & 1;
	r >>= 1;
	return std::make_pair(shouldNegate ? -r : r, Error::NoError) ;
}
Error read_relative_vlq(uint32_t& prev, std::string::const_iterator& it) {
	int32_t decoded;
	Error err;
	std::tie(decoded, err) = vlq_decode(it);
	if (err != Error::NoError)
		return err;
	int32_t v = decoded + prev;
	// TODO check too big
	if (v < 0)
		return Error::UnexpectedNegativeNumber;
	else
		prev = v;

	return Error::NoError;
}
