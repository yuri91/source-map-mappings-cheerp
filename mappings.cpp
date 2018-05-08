#include "raw_mappings.h"
#include "comparators.h"

#include <algorithm>
#include <memory>
#include <cstring>
#include <cheerp/client.h>

[[cheerp::jsexport]]
extern "C" int get_last_error() {
	return last_error;
}

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
#ifdef DEBUG
	void dump() {
		if (ptr)
			ptr->dump();
		else
			client::console.log("Invalid Mapping object");
	}
#endif
	Mapping(const RawMapping* ptr): ptr(ptr){}
private:
	const RawMapping* ptr;
};

class [[cheerp::jsexport]] [[cheerp::genericjs]] MappingsIterator {
	const RawMapping* it;
	const RawMapping* end;
public:
	MappingsIterator(const RawMapping* begin, const RawMapping* end)
		: it(begin), end(end) {}
	Mapping* next() {
		if (it != end) {
			Mapping* ret = new Mapping(it);
			it++;
			return ret;
		}
		return nullptr;
	}
};

class [[cheerp::jsexport]] [[cheerp::genericjs]] MappingsOriginalIterator {

	using LazyMappings = LazilySorted<RawMapping, cmp::ByOriginalLocation>;
	const RawMapping* it;
	const RawMapping* end;
	LazyMappings* buckets_it;
	LazyMappings* buckets_end;
public:
	MappingsOriginalIterator(LazyMappings* buckets_begin, LazyMappings* buckets_end)
		: buckets_it(buckets_begin), buckets_end(buckets_end) {
			if (buckets_it != buckets_end) {
				it = &*buckets_it->get().begin();
				end = &*buckets_it->get().end();
			} else {
				it = end = nullptr;
			}
		}
	Mapping* next() {
		while(true) {
			if (it != end) {
				Mapping* ret = new Mapping(it);
				it++;
				return ret;
			} else if (++buckets_it != buckets_end) {
				it = &*buckets_it->get().begin();
				end = &*buckets_it->get().end();
				continue;
			} else {
				return nullptr;
			}
		}
	}
};

class [[cheerp::jsexport]] [[cheerp::genericjs]] AllGeneratedLocationsFor {
	const RawMapping* it;
	const RawMapping* end;
	uint32_t line;
	bool has_column;
	uint32_t column;
public:
	AllGeneratedLocationsFor(
		const RawMapping* begin,
		const RawMapping* end,
		uint32_t original_line,
		bool has_original_column,
		uint32_t original_column
	)
		: it(begin)
		, end(end)
		, line(original_line)
		, has_column(has_original_column)
		, column(original_column)
	{
	}

	Mapping* next() {
		if (it != end) {
			if (!it->original || it->original->line != line) {
				return nullptr;
			}
			if (has_column && (!it->original || it->original->column != column))  {
				return nullptr;
			}
			Mapping* ret = new Mapping(it++);
			return ret;
		}
		return nullptr;
	}
};

class [[cheerp::jsexport]] [[cheerp::genericjs]] Mappings {
public:
	static Mappings* create(const client::String* js_input) {
		std::string input(*js_input);
		RawMappings* res = RawMappings::create(std::move(input));
		if (res)
			return new Mappings(res);
		else
			return nullptr;
	}
	void compute_column_spans() {
		ptr->compute_column_spans();
	}
	Mapping* original_location_for(
		uint32_t generated_line,
		uint32_t generated_column,
		Bias bias
	) {
		const RawMapping* ret = ptr->original_location_for(
			generated_line,
			generated_column,
			bias
		);
		if (ret==nullptr)
			return nullptr;
		return new Mapping(ret);
	}
	Mapping* generated_location_for(
		uint32_t source,
		uint32_t original_line,
		uint32_t original_column,
		Bias bias
	) {
		const RawMapping* ret = ptr->generated_location_for(
			source,
			original_line,
			original_column,
			bias
		);
		if (ret == nullptr)
			return nullptr;
		return new Mapping(ret);
	}
	AllGeneratedLocationsFor* all_generated_locations_for(
		uint32_t source,
		uint32_t original_line,
		bool has_original_column,
		uint32_t original_column
	) {
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

		auto lower = std::lower_bound(
			by_original.begin(),
			by_original.end(),
			m,
			cmp::ByOriginalLocationOnly()
		);
		MappingsIterator* iter = new MappingsIterator(&*lower, &*by_original.end());
		if (!has_original_column)
			original_line = lower->original->line;
		original_column = lower->original->column;

		return new AllGeneratedLocationsFor(
			&*lower,
			&*by_original.end(),
			original_line,
			has_original_column,
			original_column
		);
	}
	MappingsIterator* by_generated_location() {
		return new MappingsIterator(&*ptr->by_generated.begin(), &*ptr->by_generated.end());
	}
	MappingsOriginalIterator* by_original_location() {
		auto& source_buckets = ptr->source_buckets();
		return new MappingsOriginalIterator(&*source_buckets.begin(), &*source_buckets.end());
	}
	void destroy() {
		if (ptr) {
			delete ptr;
			ptr = nullptr;
		}
	}
#ifdef DEBUG
	void dump() {
		if (ptr)
			ptr->dump();
		else
			client::console.log("Invalid Mappings object");
	}
#endif
private:
	Mappings(RawMappings* ptr): ptr(ptr) {}
	RawMappings* ptr;
};


#ifdef DEBUG
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
		std::string::const_iterator it = test[i].begin();
		std::cout<<"test vlq "<<i<<":"<<test[i]<<" -> "<<vlq_decode(it)<<std::endl;
		assert(it==test[i].end());
	}

	std::cout<<"------------------------------------------"<<std::endl;
	std::string testmappings = ";EAAC,ACAA;EACA,CAAC;EACD";
	RawMappings* mappings = RawMappings::create(testmappings);
	if (last_error != Error::NoError) {
		std::cout<< "Error "<<last_error<<std::endl;
	}
	mappings->dump();
	delete mappings;
}
#endif
