#ifndef _COMPARATORS_H_
#define _COMPARATORS_H_

#include "raw_mappings.h"
#include "utils.h"

inline bool operator<(const OriginalLocation& o1, const OriginalLocation& o2) {
	return std::tie(
		o1.source,
		o1.line,
		o1.column
		//o1.name
	) < std::tie(
		o2.source,
		o2.line,
		o2.column
		//o2.name
	);
}
namespace cmp {


// NOTE: All the ByOriginalLocation... assume that r.original.has_value() == true
struct ByOriginalLocationOnly {
	inline bool operator()(const RawMapping& r1, const RawMapping& r2) {
		const OriginalLocation& o1 = *r1.original;
		const OriginalLocation& o2 = *r2.original;
		return o1 < o2;
	}
};
struct ByOriginalLocationSameSource {
	inline bool operator()(const RawMapping& r1, const RawMapping& r2) {
		const OriginalLocation& o1 = *r1.original;
		const OriginalLocation& o2 = *r2.original;
		return std::tie(
			o1.line,
			o1.column,
			r1.generated_line,
			r1.generated_column
		) < std::tie(
			o2.line,
			o2.column,
			r2.generated_line,
			r2.generated_column
		);
	}
};
struct ByOriginalLocationLineColumn {
	inline bool operator()(const RawMapping& r1, const RawMapping& r2) {
		const OriginalLocation& o1 = *r1.original;
		const OriginalLocation& o2 = *r2.original;
		return std::tie(
			o1.line,
			o1.column
		) < std::tie(
			o2.line,
			o2.column
		);
	}
};
struct ByGeneratedLocationOnly {
	inline bool operator()(const RawMapping& r1, const RawMapping& r2) const {
		return std::tie(
			r1.generated_line,
			r1.generated_column
		) < std::tie(
			r2.generated_line,
			r2.generated_column
		);
	}
};
struct ByGeneratedLocation {
	inline bool operator()(const RawMapping& r1, const RawMapping& r2) const {
		return std::tie(
			r1.generated_line,
			r1.generated_column,
			r1.original
		) < std::tie(
			r2.generated_line,
			r2.generated_column,
			r2.original
		);
	}
};
struct ByGeneratedLocationTail {
	inline bool operator()(const RawMapping& r1, const RawMapping& r2) const {
		return std::tie(
			r1.generated_column,
			r1.original
		) < std::tie(
			r2.generated_column,
			r2.original
		);
	}
};

struct Comparator {
	enum class Mode {
		ByGeneratedLocation,
		ByGeneratedLocationOnly,
		ByGeneratedLocationTail,
		ByOriginalLocationSameSource,
		ByOriginalLocationLineColumn,
		ByOriginalLocationOnly,
	};
	Mode mode;
	Comparator(Mode m = Mode::ByOriginalLocationSameSource): mode(m) {}
	bool operator()(const RawMapping& m1, const RawMapping& m2) const {
		switch (mode) {
		case Mode::ByGeneratedLocationOnly: {
			ByGeneratedLocationOnly cmp;
			return cmp(m1,m2);
		}
		case Mode::ByGeneratedLocation: {
			ByGeneratedLocation cmp;
			return cmp(m1,m2);
		}
		case Mode::ByGeneratedLocationTail: {
			ByGeneratedLocationTail cmp;
			return cmp(m1,m2);
		}
		case Mode::ByOriginalLocationOnly: {
			ByOriginalLocationOnly cmp;
			return cmp(m1,m2);
		}
		case Mode::ByOriginalLocationSameSource: {
			ByOriginalLocationSameSource cmp;
			return cmp(m1,m2);
		}
		case Mode::ByOriginalLocationLineColumn: {
			ByOriginalLocationLineColumn cmp;
			return cmp(m1,m2);
		}
		}
	}
};

}

#endif
