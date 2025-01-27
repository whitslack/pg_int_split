#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

#include <postgres.h>
#include <fmgr.h>
#include <windowapi.h>

#define _const __attribute__ ((__const__))
#define _pure __attribute__ ((__pure__))

#if UINT_MAX == UINT32_MAX
typedef div_t div32_t;
#elif ULONG_MAX == UINT32_MAX
typedef ldiv_t div32_t;
#elif ULONGLONG_MAX == UINT32_MAX
typedef lldiv_t div32_t;
#endif

#if UINT_MAX == UINT64_MAX
typedef div_t div64_t;
#elif ULONG_MAX == UINT64_MAX
typedef ldiv_t div64_t;
#elif ULONGLONG_MAX == UINT64_MAX
typedef lldiv_t div64_t;
#endif

#if defined(__GNUC__) && defined(__amd64__)

static inline div32_t _const muldiv32(int32_t multiplicand, int32_t multiplier, int32_t divisor) {
	int32_t eax, edx;
	__asm__ (".ifnc %2,%%eax\n\timull %2\n\t.else\n\timull %3\n\t.endif" : "=a,a" (eax), "=d,d" (edx) : "?0,?rm" (multiplicand), "?rm,?0" (multiplier) : "cc");
	__asm__ ("idivl %2" : "+a" (eax), "+d" (edx) : "rm" (divisor) : "cc");
	return (div32_t) { .quot = eax, .rem = edx };
}

static inline div64_t muldiv64(int64_t multiplicand, int64_t multiplier, int64_t divisor) {
	int64_t rax, rdx;
	__asm__ (".ifnc %2,%%rax\n\timulq %2\n\t.else\n\timulq %3\n\t.endif" : "=a,a" (rax), "=d,d" (rdx) : "?0,?rm" (multiplicand), "?rm,?0" (multiplier) : "cc");
	__asm__ ("idivq %2" : "+a" (rax), "+d" (rdx) : "rm" (divisor) : "cc");
	return (div64_t) { .quot = rax, .rem = rdx };
}

#else
# error "Not implemented for your CPU architecture"
#endif

#define IMPL(w) \
	struct int_split_row_##w { \
		int pos; \
		div##w##_t div; \
	}; \
	\
	static int _pure int_split_row_##w##_compare_pos(const void *p0, const void *p1) { \
		const struct int_split_row_##w *const r0 = p0, *const r1 = p1; \
		return r0->pos - r1->pos; \
	} \
	\
	static int _pure int_split_row_##w##_compare_rem(const void *p0, const void *p1) { \
		const struct int_split_row_##w *const r0 = p0, *const r1 = p1; \
		return (r0->div.rem > r1->div.rem) - (r0->div.rem < r1->div.rem); \
	} \
	\
	PG_FUNCTION_INFO_V1(window_int_split_##w); \
	Datum \
	window_int_split_##w(PG_FUNCTION_ARGS) \
	{ \
		WindowObject winobj = PG_WINDOW_OBJECT(); \
		const int64 rowcount = WinGetPartitionRowCount(winobj); \
		Assert(rowcount >= 0 && rowcount <= INT_MAX); \
		struct { \
			bool initialized; \
			struct int_split_row_##w rows[]; \
		} *mem = WinGetPartitionLocalMemory(winobj, (Size) (rowcount * sizeof(*mem->rows))); \
		struct int_split_row_##w *const rows = mem->rows; \
		if (!mem->initialized) { \
			int##w denom = 0; \
			for (int pos = 0; pos < (int) rowcount; ++pos) { \
				struct int_split_row_##w *const row = &rows[pos]; \
				bool isnull; \
				Datum datum = WinGetFuncArgInPartition(winobj, 0, pos, WINDOW_SEEK_HEAD, false, &isnull, NULL); \
				if (isnull) \
					row->div.rem = INT##w##_MIN; \
				else { \
					row->div.quot = DatumGetInt##w(datum); \
					datum = WinGetFuncArgInPartition(winobj, 1, pos, WINDOW_SEEK_HEAD, true, &isnull, NULL); \
					if (isnull) \
						row->div.rem = INT##w##_MIN; \
					else if (__builtin_add_overflow(denom, row->div.rem = DatumGetInt##w(datum), &denom)) \
						ereport(ERROR, errcode(ERRCODE_NUMERIC_VALUE_OUT_OF_RANGE), \
								errmsg("sum of split weights overflowed integer width")); \
				} \
				row->pos = pos; \
			} \
			if (denom == 0) \
				ereport(ERROR, errcode(ERRCODE_DIVISION_BY_ZERO), \
						errmsg("sum of split weights in window partition must not be zero")); \
			div##w##_t excess = { 0 }; \
			for (int pos = 0; pos < (int) rowcount; ++pos) { \
				struct int_split_row_##w *const row = &rows[pos]; \
				if (row->div.rem != INT##w##_MIN) { \
					row->div = muldiv##w(row->div.quot, row->div.rem, denom); \
					if (row->div.rem < 0) \
						row->div.rem += denom, --row->div.quot; \
					if (__builtin_add_overflow(excess.rem, row->div.rem, &excess.rem) || excess.rem >= denom) \
						excess.rem -= denom, ++excess.quot; \
				} \
			} \
			Assert(excess.rem == 0); \
			qsort(rows, (size_t) rowcount, sizeof(*rows), int_split_row_##w##_compare_rem); \
			for (int pos = 0; pos < excess.quot; ++pos) \
				++rows[pos].div.quot; \
			qsort(rows, (size_t) rowcount, sizeof(*rows), int_split_row_##w##_compare_pos); \
			mem->initialized = true; \
		} \
		const struct int_split_row_##w *const row = &rows[WinGetCurrentPosition(winobj)]; \
		if (row->div.rem == INT##w##_MIN) \
			PG_RETURN_NULL(); \
		return Int##w##GetDatum(row->div.quot); \
	}
IMPL(32)
IMPL(64)
#undef IMPL

PG_MODULE_MAGIC;
