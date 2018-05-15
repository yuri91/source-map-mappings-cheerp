#include "raw_mappings.h"
#include "comparators.h"

#include <algorithm>
#include <memory>
#include <cstring>
#include <cheerp/client.h>

enum class Order {
	Generated = 1,
	Original = 2
};

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

namespace [[cheerp::genericjs]] client {
	class MappingObject: public Object {
	public:
		void set_line(client::Object*);
		void set_column(client::Object*);
		void set_originalLine(client::Object*);
		void set_originalColumn(client::Object*);
		void set_generatedLine(client::Object*);
		void set_generatedColumn(client::Object*);
		void set_lastColumn(client::Object*);
		void set_lastGeneratedColumn(client::Object*);
		void set_source(client::String*);
		void set_name(client::String*);
	};
	class MappingCallback {
	public:
		void call(Object* context, MappingObject* mapping);
	};
}

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
	client::MappingObject* original_location_for(
		uint32_t generated_line,
		uint32_t generated_column,
		Bias bias
	) {
		const RawMapping* raw = ptr->original_location_for(
			generated_line,
			generated_column,
			bias
		);
		client::MappingObject* ret = new client::MappingObject();
		if (raw==nullptr || !raw->original) {
			ret->set_line(nullptr);
			ret->set_column(nullptr);
			ret->set_name(nullptr);
			ret->set_source(nullptr);
		} else {
			const auto& orig = raw->original;
			ret->set_line(nullable<double>(orig->line));
			ret->set_column(nullable<double>(orig->column));
			if (orig->name)
				ret->set_name(names->at(*orig->name));
			else
				ret->set_name(nullptr);
			ret->set_source(sources->at(orig->source));
		}
		return ret;
	}
	client::MappingObject* generated_location_for(
		uint32_t source,
		uint32_t original_line,
		uint32_t original_column,
		Bias bias
	) {
		const RawMapping* raw = ptr->generated_location_for(
			source,
			original_line,
			original_column,
			bias
		);
		client::MappingObject* ret = new client::MappingObject();
		if (raw == nullptr) {
			ret->set_line(nullptr);
			ret->set_column(nullptr);
			ret->set_lastColumn(nullptr);
		} else {
			ret->set_line(nullable<double>(raw->generated_line));
			ret->set_column(nullable<double>(raw->generated_column));
			nullable<double> last_column;
			if (raw->last_generated_column) {
				last_column = *raw->last_generated_column;
			}
			else if (ptr->computed_column_spans)
				last_column = std::numeric_limits<double>::infinity();
			ret->set_lastColumn(last_column);
		}
		return ret;
	}
	client::TArray<client::MappingObject>* all_generated_locations_for(
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
		if (!has_original_column)
			original_line = lower->original->line;
		original_column = lower->original->column;

		client::TArray<client::MappingObject>* ret = new client::TArray<client::MappingObject>();
		for (auto it = lower; it != by_original.end(); ++it) {
			if (!it->original)
				return ret;
			const auto& original = *it->original;
			if (original.line != original_line) {
				return ret;
			}
			if (has_original_column && original.column != original_column)  {
				return ret;
			}
			client::MappingObject* m = new client::MappingObject();
			m->set_line(nullable<double>(it->generated_line));
			m->set_column(nullable<double>(it->generated_column));
			nullable<double> last_column;
			if (it->last_generated_column) {
				last_column = *it->last_generated_column;
			}
			else if (ptr->computed_column_spans)
				last_column = std::numeric_limits<double>::infinity();
			m->set_lastColumn(last_column);
			ret->push(m);
		}
		return ret;
	}
	void each_mapping(Order order, client::Object* context, client::MappingCallback* cb) {
		auto map_to_cb = [this, cb, context](const RawMapping& raw) {
			client::MappingObject* m = new client::MappingObject();
			m->set_generatedLine(nullable<double>(raw.generated_line));
			m->set_generatedColumn(nullable<double>(raw.generated_column));
			nullable<double> last_column;
			if (raw.last_generated_column) {
				last_column = *raw.last_generated_column;
			}
			m->set_lastGeneratedColumn(last_column);
			const auto& orig = raw.original;
			if (orig) {
				m->set_originalLine(nullable<double>(orig->line));
				m->set_originalColumn(nullable<double>(orig->column));
				if (orig->name)
					m->set_name(names->at(*orig->name));
				else
					m->set_name(nullptr);
				m->set_source(sources->at(orig->source));
			} else {
				m->set_originalLine(nullptr);
				m->set_originalColumn(nullptr);
				m->set_name(nullptr);
				m->set_source(nullptr);
			}
			cb->call(context, m);
		};
		if (order == Order::Generated) {
			RawMapping* begin = &*ptr->by_generated.begin();
			RawMapping* end = &*ptr->by_generated.end();
			for (RawMapping* it = begin; it != end; ++it) {
				map_to_cb(*it);
			}
		} else if (order == Order::Original) {
			auto& source_buckets = ptr->source_buckets();
			auto* begin = &*source_buckets.begin();
			auto* end = &*source_buckets.end();
			for (auto* bucket = begin; bucket != end; ++bucket) {
				const auto* bucket_begin = &* bucket->get().begin();
				const auto* bucket_end = &*bucket->get().end();
				for (const auto* raw = bucket_begin; raw != bucket_end; ++raw) {
					map_to_cb(*raw);
				}
			}
		} else {
			throw_string("Unknown order of iteration");
		}
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
