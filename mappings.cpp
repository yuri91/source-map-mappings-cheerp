#include "raw_mappings.h"
#include "comparators.h"

#include <algorithm>
#include <memory>
#include <cstring>
#include <cheerp/client.h>

class [[cheerp::jsexport]] [[cheerp::genericjs]] Mapping {
public:
	uint32_t get_generated_line(){
		return generated_line;
	}
	uint32_t get_generated_column(){
		return generated_column;
	}
	client::String* get_source(){
		return source;
	}
	client::Object* get_original_line(){
		return original_line;
	}
	client::Object* get_original_column(){
		return original_column;
	}
	client::String* get_name() {
		return name;
	}
	client::Object* get_last_generated_column() {
		return last_generated_column;
	}
	Mapping(const RawMapping* ptr, const ArraySet* sources, const ArraySet* names)
		: generated_line(ptr->generated_line)
		, generated_column(ptr->generated_column)
	{
		if (ptr->original) {
			const auto& original = *ptr->original;
			source = sources->at(original.source);
			original_line = original.line;
			original_column = original.column;
			if (original.name)
				name = names->at(*original.name);
		}
		if (ptr->last_generated_column) {
			last_generated_column = *ptr->last_generated_column;
		}
	}
private:
	uint32_t generated_line;
	uint32_t generated_column;
	client::String* source;
	client::String* name;
	nullable<double> original_line;
	nullable<double> original_column;
	nullable<double> last_generated_column;
};

class [[cheerp::jsexport]] [[cheerp::genericjs]] MappingsIterator {
	const RawMapping* it;
	const RawMapping* end;
	const ArraySet* sources;
	const ArraySet* names;
public:
	MappingsIterator(
		const RawMapping* begin,
		const RawMapping* end,
		const ArraySet* sources,
		const ArraySet* names
	): it(begin), end(end), sources(sources), names(names) {}
	Mapping* next() {
		if (it != end) {
			Mapping* ret = new Mapping(it, sources, names);
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
	const ArraySet* sources;
	const ArraySet* names;
public:
	MappingsOriginalIterator(
		LazyMappings* buckets_begin,
		LazyMappings* buckets_end,
		const ArraySet* sources,
		const ArraySet* names
	)
		: buckets_it(buckets_begin)
		, buckets_end(buckets_end)
		, sources(sources)
		, names(names)
	{
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
				Mapping* ret = new Mapping(it, sources, names);
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
	const ArraySet* sources;
	const ArraySet* names;
public:
	AllGeneratedLocationsFor(
		const RawMapping* begin,
		const RawMapping* end,
		uint32_t original_line,
		bool has_original_column,
		uint32_t original_column,
		const ArraySet* sources,
		const ArraySet* names
	)
		: it(begin)
		, end(end)
		, line(original_line)
		, has_column(has_original_column)
		, column(original_column)
		, sources(sources)
		, names(names)
	{
	}

	Mapping* next() {
		if (it != end) {
			if (!it->original)
				return nullptr;
			const auto& original = *it->original;
			if (original.line != line) {
				return nullptr;
			}
			if (has_column && original.column != column)  {
				return nullptr;
			}
			Mapping* ret = new Mapping(it++, sources, names);
			return ret;
		}
		return nullptr;
	}
};

class [[cheerp::jsexport]] [[cheerp::genericjs]] Mappings {
public:
	static Mappings* create(
			const client::String* js_input,
			client::TArray<client::String>* sources,
			client::TArray<client::String>* names
		) {
		std::string input(*js_input);
		std::pair<RawMappings*, Error> res = RawMappings::create(std::move(input));
		if (res.second == Error::NoError)
			return new Mappings(res.first, sources, names);
		else
			throw_error(res.second);
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
		return new Mapping(ret, sources, names);
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
		return new Mapping(ret, sources, names);
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
		MappingsIterator* iter = new MappingsIterator(
			&*lower,
			&*by_original.end(),
			sources, names
		);
		if (!has_original_column)
			original_line = lower->original->line;
		original_column = lower->original->column;

		return new AllGeneratedLocationsFor(
			&*lower,
			&*by_original.end(),
			original_line,
			has_original_column,
			original_column,
			sources,
			names
		);
	}
	MappingsIterator* by_generated_location() {
		return new MappingsIterator(
			&*ptr->by_generated.begin(),
			&*ptr->by_generated.end(),
			sources, names
		);
	}
	MappingsOriginalIterator* by_original_location() {
		auto& source_buckets = ptr->source_buckets();
		return new MappingsOriginalIterator(
			&*source_buckets.begin(),
			&*source_buckets.end(),
			sources, names
		);
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
	Mappings(
		RawMappings* ptr,
		client::TArray<client::String>* sources,
		client::TArray<client::String>* names
	)	: ptr(ptr)
		, sources(new ArraySet(sources))
		, names(new ArraySet(names))
	{}
	RawMappings* ptr;
	ArraySet* sources;
	ArraySet* names;
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
	auto mappings = RawMappings::create(testmappings);
	if (mappings.second != Error::NoError) {
		delete mappings.first;
		throw_error(mappings.second);
	}
	mappings.first->dump();
	delete mappings.first;
}
#endif
