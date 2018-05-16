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

namespace [[cheerp::genericjs]] client {
	class MappingCallback {
	public:
		void call(Object* context, Object* mapping);
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
	client::Object* original_location_for(
		uint32_t generated_line,
		uint32_t generated_column,
		Bias bias
	) {
		const RawMapping* raw = ptr->original_location_for(
			generated_line,
			generated_column,
			bias
		);
		client::Object* line = nullptr;
		client::Object* column = nullptr;
		client::String* name = nullptr;
		client::String* source = nullptr;
		if (raw!=nullptr && raw->original) {
			const auto& orig = raw->original;
			line = nullable<double>(orig->line);
			column = nullable<double>(orig->column);
			if (orig->name)
				name = names->at(*orig->name);
			source = sources->at(orig->source);
		}
		return CHEERP_OBJECT(line, column, name, source);
	}
	client::Object* generated_location_for(
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
		client::Object* line = nullptr;
		client::Object* column = nullptr;
		client::Object* lastColumn = nullptr;
		if (raw != nullptr) {
			line = nullable<double>(raw->generated_line);
			column =nullable<double>(raw->generated_column);
			if (raw->last_generated_column) {
				lastColumn = nullable<double>(*raw->last_generated_column);
			}
			else if (ptr->computed_column_spans)
				lastColumn = nullable<double>(std::numeric_limits<double>::infinity());
		}
		return CHEERP_OBJECT(line, column, lastColumn);
	}
	client::TArray<client::Object>* all_generated_locations_for(
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

		client::TArray<client::Object>* ret = new client::TArray<client::Object>();
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
			double line = it->generated_line;
			double column = it->generated_column;
			client::Object* lastColumn = nullptr;
			if (it->last_generated_column) {
				lastColumn = nullable<double>(*it->last_generated_column);
			}
			else if (ptr->computed_column_spans)
				lastColumn = nullable<double>(std::numeric_limits<double>::infinity());
			ret->push(CHEERP_OBJECT(line, column, lastColumn));
		}
		return ret;
	}
	void each_mapping(Order order, client::Object* context, client::MappingCallback* cb) {
		auto map_to_cb = [this, cb, context](const RawMapping& raw) {
			client::Object* generatedLine = nullable<double>(raw.generated_line);
			client::Object* generatedColumn = nullable<double>(raw.generated_column);
			client::Object* lastGeneratedColumn = nullptr;
			if (raw.last_generated_column) {
				lastGeneratedColumn = nullable<double>(*raw.last_generated_column);
			}
			client::Object* originalLine = nullptr;
			client::Object* originalColumn = nullptr;
			client::String* source = nullptr;
			client::String* name = nullptr;
			const auto& orig = raw.original;
			if (orig) {
				originalLine = nullable<double>(orig->line);
				originalColumn = nullable<double>(orig->column);
				if (orig->name)
					name = names->at(*orig->name);
				source = sources->at(orig->source);
			}
			client::Object* m = CHEERP_OBJECT(
				generatedLine,
				generatedColumn,
				lastGeneratedColumn,
				originalLine,
				originalColumn,
				source,
				name
			);
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
