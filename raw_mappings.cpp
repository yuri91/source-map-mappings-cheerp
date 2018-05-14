#include "raw_mappings.h"
#include "comparators.h"

#include <algorithm>
#include <memory>

RawMappings::LazyMappings& RawMappings::source_buckets_slow() {
	compute_column_spans();

	LazyMappings originals;
	for (const RawMapping& m: by_generated) {
		if (!m.original)
			continue;
		if (originals.size() <= m.original->source) {
			originals.resize(m.original->source+1);
		}
		originals[m.original->source].push_back(m);
	}
	by_original = std::move(originals);
	return *by_original;
}

const RawMapping* RawMappings::original_location_for (
	uint32_t generated_line,
	uint32_t generated_column,
	Bias bias
){
	RawMapping m;
	m.generated_line = generated_line;
	m.generated_column = generated_column;
	RawMapping* ret = nullptr;
	if (bias == Bias::GreatestLowerBound) {
		auto it = std::upper_bound(
			by_generated.begin(),
			by_generated.end(),
			m,
			cmp::ByGeneratedLocationOnly()
		);
		if (it == by_generated.begin())
			return nullptr;
		ret = &*(it-1);
	} else {
		auto it = std::lower_bound(
			by_generated.begin(),
			by_generated.end(),
			m,
			cmp::ByGeneratedLocationOnly()
		);
		if (it == by_generated.end())
			return nullptr;
		ret = &*it;
	}
	if (!ret->original)
		return nullptr;
	return ret;
}

const RawMapping* RawMappings::generated_location_for (
	uint32_t source,
	uint32_t original_line,
	uint32_t original_column,
	Bias bias
) {
	auto& buckets = source_buckets();
	// TODO: original code is not doing exactly this
	if (source >= buckets.size())
		return nullptr;

	auto& by_original = buckets[source].get();
	RawMapping m;
	OriginalLocation o;
	o.source = source;
	o.line = original_line;
	o.column = original_column;
	m.original = o;

	const RawMapping* ret = nullptr;
	if (bias == Bias::GreatestLowerBound) {
		auto it = std::upper_bound(
			by_original.begin(),
			by_original.end(),
			m,
			cmp::ByOriginalLocationOnly()
		);
		if (it == by_original.begin())
			return nullptr;
		ret = &*(it-1);
	} else {
		auto it = std::lower_bound(
			by_original.begin(),
			by_original.end(),
			m,
			cmp::ByOriginalLocationOnly()
		);
		if (it == by_original.end())
			return nullptr;
		ret = &*it;
	}
	if (ret->original && ret->original->source == source)
		return ret;
	return nullptr;
}

std::pair<RawMappings*, Error> RawMappings::create(const std::string& input) {
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
	Error err = Error::NoError;

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

		err = read_relative_vlq(generated_column, it);
		if (err != Error::NoError) {
			return std::make_pair(nullptr, err);
		}
		m.generated_column = generated_column;

		if (it != in_end && *it != ';' && *it != ',') {
			OriginalLocation o;
			err = read_relative_vlq(source, it);
			if (err != Error::NoError) {
				return std::make_pair(nullptr, err);
			}
			o.source = source;
			err = read_relative_vlq(original_line, it);
			if (err != Error::NoError) {
				return std::make_pair(nullptr, err);
			}
			o.line = original_line+1;
			err = read_relative_vlq(original_column, it);
			if (err != Error::NoError) {
				return std::make_pair(nullptr, err);
			}
			o.column = original_column;
			if (it != in_end && *it != ';' && *it != ',') {
				err = read_relative_vlq(name, it);
				if (err != Error::NoError) {
					return std::make_pair(nullptr, err);
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
	return std::make_pair(mappings.release(), Error::NoError);
}
