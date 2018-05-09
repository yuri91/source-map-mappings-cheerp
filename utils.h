#ifndef _UTILS_H_
#define _UTILS_H_

#include <string>
#include <experimental/optional>

#include  <cheerp/client.h>
#include <type_traits>

template<typename T, typename std::enable_if<std::is_arithmetic<T>::value>::type* = nullptr>
class [[cheerp::genericjs]] nullable {
	client::Object* value;
public:
	inline nullable() {
		clear();
	}
	inline nullable(T i) {
		set(i);
	}
	inline nullable(client::Object* opaque): value(opaque) {}
	inline nullable(std::nullptr_t nullp): value(nullptr) {}
	operator client::Object*() {
		return get_opaque();
	}
	operator bool() {
		return is_set();
	}
	T operator*() {
		return get();
	}
private:
	inline void set(T i) {
		__asm__("%0" : "=r"(value) : "r"(i));
	}
	inline void clear() {
		__asm__("null" : "=r"(value));
	}
	inline bool is_set() {
		bool ret;
		__asm__("%0!==null" : "=r"(ret) : "r"(value));
		return ret;
	}
	inline T get() {
		T ret;
		__asm__("%0" : "=r"(ret) : "r"(value));
		return ret;
	}
	inline client::Object* get_opaque() {
		return value;
	}
};

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
std::ostream& operator<<(std::ostream& os, const indent& ind);
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
int32_t vlq_decode(std::string::const_iterator& it);
void read_relative_vlq(uint32_t& prev, std::string::const_iterator& it);

#endif
