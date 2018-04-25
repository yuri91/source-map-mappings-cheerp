#include <vector>
#include <algorithm>
#include <memory>
#include <cstring>
#include <iostream>
#include <cheerp/client.h>

enum Error {
	NoError = 0,
	UnexpectedNegativeNumber = 1,
	UnexpectedlyBigNumber = 2,
	VlqUnexpectedEof = 3,
	VlqInvalidBase64 = 4,
	VlqOverflow = 5,
};

static Error last_error = Error::NoError;

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
		std::cout<<ind<<"}"<<std::endl;
	}
	static bool comp_line_column(const RawMapping& m1, const RawMapping& m2) {
		if(m1.generated_line < m2.generated_line)
			return true;
		return m1.generated_column < m2.generated_column;
	}
	static bool comp_column(const RawMapping& m1, const RawMapping& m2) {
		if (m1.generated_column < m2.generated_column) {
			return true;
		}
		return false;
	}
};
struct RawMappings {
	std::vector<RawMapping> by_generated;
	bool computed_column_spans{false};
	void dump(indent ind = indent(0)) const {
		std::cout<<ind<<"Mappings ["<<std::endl;
		for(const auto& m: by_generated) {
			std::cout << ind;
			m.dump(ind.inc());
		}
		std::cout<<ind<<"]"<<std::endl;
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

	for (auto it = in_begin; it != in_end;) {
		if (*it ==  ';') {
			generated_line++;
			generated_column = 0;
			it++;
			if (generated_line_start_index < by_generated.size()) {
				std::sort(by_generated.begin()+generated_line_start_index,
				          by_generated.end(),
				          RawMapping::comp_column);
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
		          RawMapping::comp_column);
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
		                           m, RawMapping::comp_line_column);
		if (it == ptr->by_generated.end())
			return nullptr;
		if (!it->has_original_location)
			return nullptr;
		// TODO bias
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
	//Mappings(): ptr(nullptr) {}
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
