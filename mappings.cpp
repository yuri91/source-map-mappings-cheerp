#include <vector>
#include <algorithm>
#include <memory>
#include <cstring>
#include <iostream>
#include <experimental/optional>
#include <cheerp/client.h>

template<class T>
using optional = std::experimental::optional<T>;

enum Error {
	NoError = 0,
	UnexpectedNegativeNumber = 1,
	UnexpectedlyBigNumber = 2,
	VlqUnexpectedEof = 3,
	VlqInvalidBase64 = 4,
	VlqOverflow = 5,
};

static Error last_error = Error::NoError;

template<class T, class C>
class LazilySorted {
	bool sorted;
	std::vector<T> vec;
	C cmp;

public:
	LazilySorted(): sorted(false), cmp() {}
	const std::vector<T>& get() {
		if(sorted)
			return vec;
		std::sort(vec.begin(), vec.end(), cmp);
		return vec;
	}
	void push_back(T elem) {
		sorted = false;
		vec.push_back(elem);
	}
};

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
struct RawMapping {
	uint32_t generated_line{0};
	uint32_t generated_column{0};
	bool has_original_location{false};
	uint32_t source{0};
	uint32_t original_line{0};
	uint32_t original_column{0};
	bool has_name{false};
	uint32_t name{0};
	optional<uint32_t> last_generated_column;
	void dump(indent ind = indent(0)) const {
		std::cout
			<<ind<<"Mapping {" << std::endl
			<<ind.inc()<<"generated_line: "<<generated_line<<std::endl
			<<ind.inc()<<"generated_column: "<<generated_column<<std::endl;
		if (has_original_location) {
			std::cout
				<<ind.inc()<<"original_line: "<<original_line<<std::endl
				<<ind.inc()<<"original_column: "<<original_column<<std::endl;
		}
		if (has_name) {
			std::cout
				<<ind.inc()<<"name: "<<name<<std::endl;
		}
		if (last_generated_column) {
			std::cout
				<<ind.inc()<<"last_generated_column: "<<*last_generated_column<<std::endl;
		}
		std::cout<<ind<<"}"<<std::endl;
	}
};
namespace cmp {
	enum class Ordering {
		Less = -1,
		Equal = 0,
		Greater = 1,
	};
	inline Ordering cmp(uint32_t u1, uint32_t u2) {
		if (u1 < u2)
			return Ordering::Less;
		if (u1 > u2)
			return Ordering::Greater;
		return Ordering::Equal;
	}
	struct ByOriginalLocation {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			switch (cmp(m1.source, m2.source)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.original_line, m2.original_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.original_column, m2.original_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.name, m2.name)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.generated_line, m2.generated_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			if (cmp(m1.generated_column, m2.generated_column) == Ordering::Less)
				return true;
			return false;
		}
	};
	struct ByGeneratedLocation {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			switch (cmp(m1.generated_line, m2.generated_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.generated_column, m2.generated_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.source, m2.source)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.original_line, m2.original_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.original_column, m2.original_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			if (cmp(m1.name, m2.name) == Ordering::Less)
				return true;
			return false;
		}
	};
	struct ByGeneratedLocationTail {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			switch (cmp(m1.generated_column, m2.generated_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.source, m2.source)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.original_line, m2.original_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (cmp(m1.original_column, m2.original_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			if (cmp(m1.name, m2.name) == Ordering::Less)
				return true;
			return false;
		}
	};
}
struct RawMappings {
	std::vector<RawMapping> by_generated;
	bool computed_column_spans{false};
	optional<std::vector<LazilySorted<RawMapping, cmp::ByOriginalLocation>>> by_original;
	void dump(indent ind = indent(0)) const {
		std::cout<<ind<<"Mappings ["<<std::endl;
		for(const auto& m: by_generated) {
			std::cout << ind;
			m.dump(ind.inc());
		}
		std::cout<<ind<<"]"<<std::endl;
	}
	std::vector<LazilySorted<RawMapping, cmp::ByOriginalLocation>>& source_buckets() {
		if (by_original) {
			return *by_original;
		}

		compute_column_spans();

		std::vector<LazilySorted<RawMapping, cmp::ByOriginalLocation>> originals;
		for (const RawMapping& m: by_generated) {
			if (!m.has_original_location)
				continue;
			while (originals.size() <= m.source) {
				originals.push_back(LazilySorted<RawMapping, cmp::ByOriginalLocation>());
			}
			originals[m.source].push_back(m);
		}
		by_original = std::move(originals);
		return *by_original;
	}
private:
	void compute_column_spans() {
		if (computed_column_spans)
			return;
		auto it = by_generated.begin();
		while (it != by_generated.end()) {
			RawMapping& m = *it++;
			if (it != by_generated.end()) {
				m.last_generated_column = it->generated_column;
			}
		}
		computed_column_spans = true;
	}
};

class [[cheerp::jsexport]] [[cheerp::genericjs]] Mapping {
public:
	uint32_t generated_line(){
		return ptr->generated_line;
	}
	uint32_t generated_column(){
		return ptr->generated_column;
	}
	bool has_original_location(){
		return ptr->has_original_location;
	}
	uint32_t source(){
		return ptr->source;
	}
	uint32_t original_line(){
		return ptr->original_line;
	}
	uint32_t original_column(){
		return ptr->original_column;
	}
	bool has_name(){
		return ptr->has_name;
	}
	uint32_t name() {
		return ptr->name;
	}
	bool has_last_generated_column() {
		return bool(ptr->last_generated_column);
	}
	uint32_t last_generated_column() {
		return *ptr->last_generated_column;
	}
	void dump() {
		if (ptr)
			ptr->dump();
		else
			client::console.log("Invalid Mapping object");
	}
	Mapping(const RawMapping* ptr): ptr(ptr){}
private:
	const RawMapping* ptr;
};

static uint32_t base64_decode(char in) {
	// TODO error handling
	if (in == '+') {
		return 62;
	} else if (in == '/') {
		return 63;
	} else if (in >= 'A' && in <= 'Z') {
		return in - 'A';
	} else if (in >= 'a' && in <= 'z') {
		return in - 'a' + ('z' - 'a') + 1;
	} else {
		return in - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
	}
}
static int64_t vlq_decode(std::string::iterator& it) {
	int64_t r = 0;
	uint32_t shift = 0;
	bool hasContinuationBit = false;
	do {
		uint32_t i = base64_decode(*it);
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
static void read_relative_vlq(uint32_t& prev, std::string::iterator& it) {
	int64_t decoded = vlq_decode(it);
	// TODO check error
	int64_t v = decoded + prev;
	// TODO check too big
	if (v < 0)
		last_error = Error::UnexpectedNegativeNumber;
	else
		prev = v;
}

[[cheerp::jsexport]]
extern "C" int get_last_error() {
	return last_error;
}

static RawMappings* parse_mappings_impl(std::string input) {
	std::unique_ptr<RawMappings> mappings = std::make_unique<RawMappings>();

	auto in_begin = input.begin();
	uint32_t in_len = input.size();
	auto in_end = input.end();
	uint32_t generated_line = 0;
	uint32_t generated_column = 0;
	uint32_t original_line = 0;
	uint32_t original_column = 0;
	uint32_t source = 0;
	uint32_t name = 0;
	uint32_t generated_line_start_index = 0;

	std::vector<RawMapping> by_generated;
	by_generated.reserve(in_len / 2);

	cmp::ByGeneratedLocationTail comparator;
	for (auto it = in_begin; it != in_end;) {
		if (*it ==  ';') {
			generated_line++;
			generated_column = 0;
			it++;
			if (generated_line_start_index < by_generated.size()) {
				std::sort(by_generated.begin()+generated_line_start_index,
				          by_generated.end(),
				          comparator);
				generated_line_start_index = by_generated.size();
			}
			continue;
		} else if (*it == ',') {
			it++;
			continue;
		}
		RawMapping m;
		m.generated_line = generated_line;

		read_relative_vlq(generated_column, it);
		if (last_error != Error::NoError) {
			return nullptr;
		}
		m.generated_column = generated_column;

		if (it != in_end && *it != ';' && *it != ',') {
			m.has_original_location = true;
			read_relative_vlq(source, it);
			if (last_error != Error::NoError) {
				return nullptr;
			}
			m.source = source;
			read_relative_vlq(original_line, it);
			if (last_error != Error::NoError) {
				return nullptr;
			}
			m.original_line = original_line;
			read_relative_vlq(original_column, it);
			if (last_error != Error::NoError) {
				return nullptr;
			}
			m.original_column = original_column;
			if (it != in_end && *it != ';' && *it != ',') {
				m.has_name = true;
				read_relative_vlq(name, it);
				if (last_error != Error::NoError) {
					return nullptr;
				}
				m.name = name;
			}
		}
		by_generated.push_back(m);
	}
	if (generated_line_start_index < by_generated.size()) {
		std::sort(by_generated.begin()+generated_line_start_index,
		          by_generated.end(),
		          comparator);
	}
	mappings->by_generated = std::move(by_generated);
	return mappings.release();
}

class [[cheerp::jsexport]] [[cheerp::genericjs]] Mappings {
public:
	static Mappings* create(const client::String* js_input) {
		std::string input(*js_input);
		RawMappings* res = parse_mappings_impl(std::move(input));
		if (res)
			return new Mappings(res);
		else
			return nullptr;
	}
	Mapping* original_location_for(uint32_t generated_line, uint32_t generated_column) {
		RawMapping m;
		m.generated_line = generated_line;
		m.generated_column = generated_column;
		auto it = std::lower_bound(ptr->by_generated.begin(), ptr->by_generated.end(),
		                           m, cmp::ByGeneratedLocation());
		if (it == ptr->by_generated.end())
			return nullptr;
		if (!it->has_original_location)
			return nullptr;
		// TODO configurable bias
		return new Mapping(&*it);
	}
	Mapping* generated_location_for(uint32_t source, uint32_t original_line, uint32_t original_column) {
		auto& source_buckets = ptr->source_buckets();
		// TODO: original code is not doing exactly this
		if (source >= source_buckets.size())
			return nullptr;

		auto& by_original = source_buckets[source].get();
		RawMapping m;
		m.has_original_location = true;
		m.source = source;
		m.original_line = original_line;
		m.original_column = original_column;

		auto it = std::lower_bound(by_original.begin(), by_original.end(), m, cmp::ByOriginalLocation());
		if (it == by_original.end())
			return nullptr;
		return new Mapping(&*it);
	}
	void destroy() {
		if (ptr) {
			delete ptr;
			ptr = nullptr;
		}
	}
	void dump() {
		if (ptr)
			ptr->dump();
		else
			client::console.log("Invalid Mappings object");
	}
private:
	Mappings(RawMappings* ptr): ptr(ptr) {}
	RawMappings* ptr;
};


[[cheerp::jsexport]]
extern "C"  void test() {
	char testbase[] = {'A', 'Z', 'a', 'z', '0', '9', '/'};

	std::cout<<"------------------------------------------"<<std::endl;
	for(int i = 0; i < 7; i++) {
		std::cout
			<<"test base64 "<<i<<":"
			<<testbase[i]<<" -> "
			<<base64_decode(testbase[i])
			<<std::endl;
	}
	std::cout<<"------------------------------------------"<<std::endl;

	std::string test[] = {"A", "C", "D", "2H", "qxmvrH"};

	for(int i = 0; i < 5; i++) {
		auto it = test[i].begin();
		std::cout<<"test vlq "<<i<<":"<<test[i]<<" -> "<<vlq_decode(it)<<std::endl;
		assert(it==test[i].end());
	}

	std::cout<<"------------------------------------------"<<std::endl;
	std::string testmappings = ";EAAC,ACAA;EACA,CAAC;EACD";
	RawMappings* mappings = parse_mappings_impl(testmappings);
	if (last_error != Error::NoError) {
		std::cout<< "Error "<<last_error<<std::endl;
	}
	mappings->dump();
	delete mappings;
}
