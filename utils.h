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

namespace [[cheerp::genericjs]] client {
	class Map: public Object {
	public:
		Map();
		template<typename K, typename V>
		void set(K k, V v);
		template<typename K, typename V>
		V get(K k);
		template<typename K>
		bool has(K k);
	};
	template<typename K, typename V>
	class TMap: public Map {
	public:
		TMap(): Map() {}
		void set(K k, V v) {
			Map::set<K,V>(k,v);
		}
		V get(K k) {
			return Map::get<K,V>(k);
		}
		bool has(K k) {
			return Map::has<K>(k);
		}
	};
}
class [[cheerp::genericjs]] ArraySet {
	client::TArray<client::String>* array;
	client::TMap<client::String*, uint32_t>* map;
public:
	ArraySet()
		: array(new client::TArray<client::String>())
		, map(new client::TMap<client::String*, uint32_t>())
	{}
	ArraySet(client::TArray<client::String>* arr)
		: array(arr)
		, map(new client::TMap<client::String*, uint32_t>()) {
		for (int i = 0; i < array->get_length(); i++) {
			map->set((*array)[i], i);
		}
	}
	void add(client::String* s, bool allow_duplicate = false) {
		uint32_t idx = array->get_length();
		bool is_duplicate = map->has(s);
		if (!is_duplicate || allow_duplicate)
			array->push(s);
		if (!is_duplicate)
			map->set(s, idx);
	}
	client::String* at(uint32_t idx) {
		if (idx < array->get_length()) {
			return (*array)[idx];
		}
		return nullptr;
	}
	uint32_t index_of(client::String* s) {
		if (map->has(s))
			return map->get(s);
		client::String* err = new client::String("'");
		err = err->concat(s);
		err = err->concat("' is not in the set");
		__asm__("throw new Error(%0)" : : "r"(err));
		return 0;
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
[[cheerp::genericjs]]
[[noreturn]]
void throw_error(Error e);

int32_t base64_decode(char in);
std::pair<int32_t, Error> vlq_decode(std::string::const_iterator& it);
Error read_relative_vlq(uint32_t& prev, std::string::const_iterator& it);

#endif
