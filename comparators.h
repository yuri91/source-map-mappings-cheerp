#ifndef _COMPARATORS_H_
#define _COMPARATORS_H_

#include "raw_mappings.h"
#include "utils.h"

namespace cmp {

enum class Ordering {
	Less = -1,
	Equal = 0,
	Greater = 1,
};
template<class T>
struct Orderer {
	Ordering operator()(const T& u1, const T& u2) const {
		if (u1 < u2)
			return Ordering::Less;
		if (u1 > u2)
			return Ordering::Greater;
		return Ordering::Equal;
	}
};
#define CMP_ORD(o1, o2)\
{\
	Orderer<decltype(o1)> ord;\
	auto r = ord(o1, o2);\
	if (r != Ordering::Equal)\
		return r;\
}
#define CMP_BOOL(o1, o2)\
{\
	Orderer<decltype(o1)> ord;\
	auto r = ord(o1, o2);\
	if (r != Ordering::Equal)\
		return r == Ordering::Less;\
}

template<class T>
struct Orderer<optional<T>> {
	Ordering operator()(
		const optional<T>& u1,
		const optional<T>& u2
	) const {
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
	Ordering operator()(
		const OriginalLocation& o1,
		const OriginalLocation& o2
	) const {
		CMP_ORD(o1.source, o2.source);
		CMP_ORD(o1.line, o2.line);
		CMP_ORD(o1.column, o2.column);
		// We never compare names
		return Ordering::Equal;
	}
};
struct ByOriginalLocationOnly {
	bool operator()(const RawMapping& m1, const RawMapping& m2) const {
		Orderer<optional<OriginalLocation>> ord_orig;
		return ord_orig(m1.original, m2.original) == Ordering::Less;
	}
};
struct ByOriginalLocation {
	bool operator()(const RawMapping& m1, const RawMapping& m2) const {
		CMP_BOOL(m1.original, m2.original);
		CMP_BOOL(m1.generated_line, m2.generated_line);
		return m1.generated_column < m2.generated_column;
	}
};
struct ByGeneratedLocationOnly {
	bool operator()(const RawMapping& m1, const RawMapping& m2) const {
		CMP_BOOL(m1.generated_line, m2.generated_line);
		return m1.generated_column < m2.generated_column;
	}
};
struct ByGeneratedLocation {
	bool operator()(const RawMapping& m1, const RawMapping& m2) const {
		CMP_BOOL(m1.generated_line, m2.generated_line);
		CMP_BOOL(m1.generated_column, m2.generated_column);
		CMP_BOOL(m1.original, m2.original);
		return false;
	}
};
struct ByGeneratedLocationTail {
	bool operator()(const RawMapping& m1, const RawMapping& m2) const {
		CMP_BOOL(m1.generated_column, m2.generated_column);
		CMP_BOOL(m1.original, m2.original);
		return false;
	}
};

struct Comparator {
	enum class Mode {
		ByGeneratedLocation,
		ByGeneratedLocationOnly,
		ByGeneratedLocationTail,
		ByOriginalLocation,
		ByOriginalLocationOnly,
	};
	Mode mode;
	Comparator(Mode m = Mode::ByOriginalLocation): mode(m) {}
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
		case Mode::ByOriginalLocation: {
			ByOriginalLocation cmp;
			return cmp(m1,m2);
		}
		}
	}
};

}

#endif
