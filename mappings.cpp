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

enum class Bias {
	GreatestLowerBound = 1,
	LeastUpperBound = 2,
};

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
struct OriginalLocation {
	uint32_t source{0};
	uint32_t line{0};
	uint32_t column{0};
	optional<uint32_t> name;
};
struct RawMapping {
	uint32_t generated_line{0};
	uint32_t generated_column{0};
	optional<OriginalLocation> original;
	optional<uint32_t> last_generated_column;
	void dump(indent ind = indent(0)) const {
		std::cout
			<<ind<<"Mapping {" << std::endl
			<<ind.inc()<<"generated_line: "<<generated_line<<std::endl
			<<ind.inc()<<"generated_column: "<<generated_column<<std::endl;
		if (original) {
			std::cout
				<<ind.inc()<<"original_line: "<<original->line<<std::endl
				<<ind.inc()<<"original_column: "<<original->column<<std::endl;
			if (original->name) {
				std::cout
					<<ind.inc()<<"name: "<<*original->name<<std::endl;
			}
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
	template<class T>
	struct Orderer {
		inline Ordering operator()(const T& u1, const T& u2) {
			if (u1 < u2)
				return Ordering::Less;
			if (u1 > u2)
				return Ordering::Greater;
			return Ordering::Equal;
		}
	};
	template<class T>
	struct Orderer<optional<T>> {
		inline Ordering operator()(const optional<T>& u1, const optional<T>& u2) {
			Orderer<T> ord;
			if (bool(u1) && bool(u2))
				return ord(*u1, *u2);
			if (!bool(u1) && !bool(u2))
				return Ordering::Equal;
			if (!bool(u1))
				return Ordering::Less;
			return Ordering::Greater;
		}
	};
	template<>
	struct Orderer<OriginalLocation> {
		inline Ordering operator()(const OriginalLocation& o1, const OriginalLocation& o2) {
			Orderer<uint32_t> ord;
			switch (ord(o1.source, o2.source)) {
			case Ordering::Less:
				return Ordering::Less;
			case Ordering::Greater:
				return Ordering::Greater;
			default:
				break;
			}
			switch (ord(o1.line, o2.line)) {
			case Ordering::Less:
				return Ordering::Less;
			case Ordering::Greater:
				return Ordering::Greater;
			default:
				break;
			}
			switch (ord(o1.column, o2.column)) {
			case Ordering::Less:
				return Ordering::Less;
			case Ordering::Greater:
				return Ordering::Greater;
			default:
				break;
			}
			// We never compare names
			return Ordering::Equal;
		}
	};
	struct ByOriginalLocationOnly {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			Orderer<optional<OriginalLocation>> ord_orig;
			Orderer<uint32_t> ord;
			switch (ord_orig(m1.original, m2.original)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			return false;
		}
	};
	struct ByOriginalLocation {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			Orderer<optional<OriginalLocation>> ord_orig;
			Orderer<uint32_t> ord;
			switch (ord_orig(m1.original, m2.original)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (ord(m1.generated_line, m2.generated_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			if (ord(m1.generated_column, m2.generated_column) == Ordering::Less)
				return true;
			return false;
		}
	};
	struct ByGeneratedLocationOnly {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			Orderer<uint32_t> ord;
			switch (ord(m1.generated_line, m2.generated_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (ord(m1.generated_column, m2.generated_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			return false;
		}
	};
	struct ByGeneratedLocation {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			Orderer<optional<OriginalLocation>> ord_orig;
			Orderer<uint32_t> ord;
			switch (ord(m1.generated_line, m2.generated_line)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (ord(m1.generated_column, m2.generated_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (ord_orig(m1.original, m2.original)) {
			case Ordering::Less:
				return true;
			default:
				return false;
			}
		}
	};
	struct ByGeneratedLocationTail {
		bool operator()(const RawMapping& m1, const RawMapping& m2) {
			Orderer<optional<OriginalLocation>> ord_orig;
			Orderer<uint32_t> ord;
			switch (ord(m1.generated_column, m2.generated_column)) {
			case Ordering::Less:
				return true;
			case Ordering::Greater:
				return false;
			default:
				break;
			}
			switch (ord_orig(m1.original, m2.original)) {
			case Ordering::Less:
				return true;
			default:
				return false;
			}
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
			if (!m.original)
				continue;
			while (originals.size() <= m.original->source) {
				originals.push_back(LazilySorted<RawMapping, cmp::ByOriginalLocation>());
			}
			originals[m.original->source].push_back(m);
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
		return bool(ptr->original);
	}
	uint32_t source(){
		return ptr->original->source;
	}
	uint32_t original_line(){
		return ptr->original->line;
	}
	uint32_t original_column(){
		return ptr->original->column;
	}
	bool has_name(){
		return bool(ptr->original) && bool(ptr->original->name);
	}
	uint32_t name() {
		return *ptr->original->name;
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
	uint32_t generated_line = 1;
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
			OriginalLocation o;
			read_relative_vlq(source, it);
			if (last_error != Error::NoError) {
				return nullptr;
			}
			o.source = source;
			read_relative_vlq(original_line, it);
			if (last_error != Error::NoError) {
				return nullptr;
			}
			o.line = original_line+1;
			read_relative_vlq(original_column, it);
			if (last_error != Error::NoError) {
				return nullptr;
			}
			o.column = original_column;
			if (it != in_end && *it != ';' && *it != ',') {
				read_relative_vlq(name, it);
				if (last_error != Error::NoError) {
					return nullptr;
				}
				o.name = name;
			}
			m.original = o;
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

class [[cheerp::jsexport]] [[cheerp::genericjs]] MappingsIterator {
	RawMapping* it;
	RawMapping* end;
public:
	MappingsIterator(RawMapping* begin, RawMapping* end): it(begin), end(end) {}
	Mapping* next() {
		if (it != end) {
			Mapping* ret = new Mapping(it);
			it++;
			return ret;
		}
		return nullptr;
	}
};

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
	Mapping* original_location_for(uint32_t generated_line,
	                               uint32_t generated_column,
	                               Bias bias) {
		RawMapping m;
		m.generated_line = generated_line;
		m.generated_column = generated_column;
		RawMapping* ret = nullptr;
		if (bias == Bias::GreatestLowerBound) {
			auto it = std::upper_bound(ptr->by_generated.begin(),
			                           ptr->by_generated.end(),
			                           m, cmp::ByGeneratedLocationOnly());
			if (it == ptr->by_generated.begin())
				return nullptr;
			ret = &*(it-1);
		} else {
			auto it = std::lower_bound(ptr->by_generated.begin(),
			                           ptr->by_generated.end(),
			                           m, cmp::ByGeneratedLocationOnly());
			if (it == ptr->by_generated.end())
				return nullptr;
			ret = &*it;
		}
		if (!ret->original)
			return nullptr;
		return new Mapping(ret);
	}
	Mapping* generated_location_for(uint32_t source,
	                                uint32_t original_line,
	                                uint32_t original_column,
	                                Bias bias) {
		auto& source_buckets = ptr->source_buckets();
		// TODO: original code is not doing exactly this
		if (source >= source_buckets.size())
			return nullptr;

		auto& by_original = source_buckets[source].get();
		RawMapping m;
		OriginalLocation o;
		o.source = source;
		o.line = original_line;
		o.column = original_column;
		m.original = o;

		const RawMapping* ret = nullptr;
		if (bias == Bias::GreatestLowerBound) {
			auto it = std::upper_bound(by_original.begin(),
			                           by_original.end(),
			                           m, cmp::ByOriginalLocationOnly());
			if (it == by_original.begin())
				return nullptr;
			ret = &*(it-1);
		} else {
			auto it = std::lower_bound(by_original.begin(),
			                           by_original.end(),
			                           m, cmp::ByOriginalLocationOnly());
			if (it == by_original.end())
				return nullptr;
			ret = &*it;
		}
		return new Mapping(ret);
	}
	MappingsIterator* by_generated_location() {
		return new MappingsIterator(&*ptr->by_generated.begin(), &*ptr->by_generated.end());
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
