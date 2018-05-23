#ifndef _RAW_MAPPINGS_H_
#define _RAW_MAPPINGS_H_

#include "utils.h"

#include <vector>

enum class Bias {
	GreatestLowerBound = 1,
	LeastUpperBound = 2,
};

template<class T, class C>
class LazilySorted {
public:
	LazilySorted(): sorted(false) {}
	const std::vector<T>& get() {
		if(sorted)
			return vec;
		std::sort(vec.begin(), vec.end(), C());
		sorted = true;
		return vec;
	}
	void push_back(T elem) {
		sorted = false;
		vec.push_back(elem);
	}
private:
	bool sorted;
	std::vector<T> vec;
};

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
#ifdef DEBUG
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
#endif
};
namespace cmp {
	class Comparator;
}
class RawMappings {
public:
	using LazyMappings = std::vector<
		LazilySorted<RawMapping, cmp::Comparator>
	>;
#ifdef DEBUG
	void dump(indent ind = indent(0)) const {
		std::cout<<ind<<"Mappings ["<<std::endl;
		for(const auto& m: by_generated) {
			std::cout << ind;
			m.dump(ind.inc());
		}
		std::cout<<ind<<"]"<<std::endl;
	}
#endif
	void compute_column_spans() {
		if (computed_column_spans)
			return;
		auto it = by_generated.begin();
		while (it != by_generated.end()) {
			RawMapping& m = *it++;
			if (it != by_generated.end() && it->generated_line == m.generated_line) {
				m.last_generated_column = it->generated_column - 1;
			}
		}
		computed_column_spans = true;
	}
	const RawMapping* original_location_for (
		uint32_t generated_line,
		uint32_t generated_column,
		Bias bias
	);
	const RawMapping* generated_location_for (
		uint32_t source,
		uint32_t original_line,
		uint32_t original_column,
		Bias bias
	);
	static std::pair<RawMappings*, Error> create(const std::string& input);
	LazyMappings& source_buckets() {
		if (by_original) {
			return *by_original;
		}
		source_buckets_slow();
		return *by_original;
	}
	std::vector<RawMapping> by_generated;
	bool computed_column_spans{false};
private:
	LazyMappings& source_buckets_slow();

	optional<LazyMappings> by_original;
};

#endif
