#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>
#include <experimental/optional>

template<class T>
using optional = std::experimental::optional<T>;


#ifdef DEBUG
#include <iostream>

struct indent {
	int level{0};
	indent(int level): level(level){}
	indent inc() {
		indent i = *this;
		i.level++;
		return i;
	}
};
std::ostream& operator<<(std::ostream& os, const indent& ind) {
	for (int i = 0; i < ind.level; i++) {
		os << '\t';
	}
	return os;
}
#endif

enum Error {
	NoError = 0,
	UnexpectedNegativeNumber = 1,
	UnexpectedlyBigNumber = 2,
	VlqUnexpectedEof = 3,
	VlqInvalidBase64 = 4,
	VlqOverflow = 5,
};
extern Error last_error;

int32_t base64_decode(char in);
int64_t vlq_decode(std::string::const_iterator& it);
void read_relative_vlq(uint32_t& prev, std::string::const_iterator& it);

#endif
