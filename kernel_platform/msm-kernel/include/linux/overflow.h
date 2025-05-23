/* SPDX-License-Identifier: GPL-2.0 OR MIT */
#ifndef __LINUX_OVERFLOW_H
#define __LINUX_OVERFLOW_H

#include <linux/compiler.h>
#include <linux/limits.h>

/*
 * We need to compute the minimum and maximum values representable in a given
 * type. These macros may also be useful elsewhere. It would seem more obvious
 * to do something like:
 *
 * #define type_min(T) (T)(is_signed_type(T) ? (T)1 << (8*sizeof(T)-1) : 0)
 * #define type_max(T) (T)(is_signed_type(T) ? ((T)1 << (8*sizeof(T)-1)) - 1 : ~(T)0)
 *
 * Unfortunately, the middle expressions, strictly speaking, have
 * undefined behaviour, and at least some versions of gcc warn about
 * the type_max expression (but not if -fsanitize=undefined is in
 * effect; in that case, the warning is deferred to runtime...).
 *
 * The slightly excessive casting in type_min is to make sure the
 * macros also produce sensible values for the exotic type _Bool. [The
 * overflow checkers only almost work for _Bool, but that's
 * a-feature-not-a-bug, since people shouldn't be doing arithmetic on
 * _Bools. Besides, the gcc builtins don't allow _Bool* as third
 * argument.]
 *
 * Idea stolen from
 * https://mail-index.netbsd.org/tech-misc/2007/02/05/0000.html -
 * credit to Christian Biere.
 */
#define is_signed_type(type)       (((type)(-1)) < (type)1)
#define __type_half_max(type) ((type)1 << (8*sizeof(type) - 1 - is_signed_type(type)))
#define type_max(T) ((T)((__type_half_max(T) - 1) + __type_half_max(T)))
#define type_min(T) ((T)((T)-type_max(T)-(T)1))

/*
 * Avoids triggering -Wtype-limits compilation warning,
 * while using unsigned data types to check a < 0.
 */
#define is_non_negative(a) ((a) > 0 || (a) == 0)
#define is_negative(a) (!(is_non_negative(a)))

/*
 * Allows for effectively applying __must_check to a macro so we can have
 * both the type-agnostic benefits of the macros while also being able to
 * enforce that the return value is, in fact, checked.
 */
static inline bool __must_check __must_check_overflow(bool overflow)
{
	return unlikely(overflow);
}

/** check_add_overflow() - Calculate addition with overflow checking
 *
 * @a: first addend
 * @b: second addend
 * @d: pointer to store sum
 *
 * Returns 0 on success.
 *
 * *@d holds the results of the attempted addition, but is not considered
 * "safe for use" on a non-zero return value, which indicates that the
 * sum has overflowed or been truncated.
 */
#define check_add_overflow(a, b, d)	\
	__must_check_overflow(__builtin_add_overflow(a, b, d))

/** check_sub_overflow() - Calculate subtraction with overflow checking
 *
 * @a: minuend; value to subtract from
 * @b: subtrahend; value to subtract from @a
 * @d: pointer to store difference
 *
 * Returns 0 on success.
 *
 * *@d holds the results of the attempted subtraction, but is not considered
 * "safe for use" on a non-zero return value, which indicates that the
 * difference has underflowed or been truncated.
 */
#define check_sub_overflow(a, b, d)	\
	__must_check_overflow(__builtin_sub_overflow(a, b, d))

/** check_mul_overflow() - Calculate multiplication with overflow checking
 *
 * @a: first factor
 * @b: second factor
 * @d: pointer to store product
 *
 * Returns 0 on success.
 *
 * *@d holds the results of the attempted multiplication, but is not
 * considered "safe for use" on a non-zero return value, which indicates
 * that the product has overflowed or been truncated.
 */
#define check_mul_overflow(a, b, d)	\
	__must_check_overflow(__builtin_mul_overflow(a, b, d))

/** check_shl_overflow() - Calculate a left-shifted value and check overflow
 *
 * @a: Value to be shifted
 * @s: How many bits left to shift
 * @d: Pointer to where to store the result
 *
 * Computes *@d = (@a << @s)
 *
 * Returns true if '*d' cannot hold the result or when 'a << s' doesn't
 * make sense. Example conditions:
 * - 'a << s' causes bits to be lost when stored in *d.
 * - 's' is garbage (e.g. negative) or so large that the result of
 *   'a << s' is guaranteed to be 0.
 * - 'a' is negative.
 * - 'a << s' sets the sign bit, if any, in '*d'.
 *
 * '*d' will hold the results of the attempted shift, but is not
 * considered "safe for use" if true is returned.
 */
#define check_shl_overflow(a, s, d) __must_check_overflow(({		\
	typeof(a) _a = a;						\
	typeof(s) _s = s;						\
	typeof(d) _d = d;						\
	u64 _a_full = _a;						\
	unsigned int _to_shift =					\
		is_non_negative(_s) && _s < 8 * sizeof(*d) ? _s : 0;	\
	*_d = (_a_full << _to_shift);					\
	(_to_shift != _s || is_negative(*_d) || is_negative(_a) ||	\
	(*_d >> _to_shift) != _a);					\
}))

/**
 * size_mul() - Calculate size_t multiplication with saturation at SIZE_MAX
 *
 * @factor1: first factor
 * @factor2: second factor
 *
 * Returns: calculate @factor1 * @factor2, both promoted to size_t,
 * with any overflow causing the return value to be SIZE_MAX. The
 * lvalue must be size_t to avoid implicit type conversion.
 */
static inline size_t __must_check size_mul(size_t factor1, size_t factor2)
{
	size_t bytes;

	if (check_mul_overflow(factor1, factor2, &bytes))
		return SIZE_MAX;

	return bytes;
}



/**
 * size_sub() - Calculate size_t subtraction with saturation at SIZE_MAX
 *
 * @minuend: value to subtract from
 * @subtrahend: value to subtract from @minuend
 *
 * Returns: calculate @minuend - @subtrahend, both promoted to size_t,
 * with any overflow causing the return value to be SIZE_MAX. For
 * composition with the size_add() and size_mul() helpers, neither
 * argument may be SIZE_MAX (or the result with be forced to SIZE_MAX).
 * The lvalue must be size_t to avoid implicit type conversion.
 */
static inline size_t __must_check size_sub(size_t minuend, size_t subtrahend)
{
	size_t bytes;

	if (minuend == SIZE_MAX || subtrahend == SIZE_MAX ||
	    check_sub_overflow(minuend, subtrahend, &bytes))
		return SIZE_MAX;

	return bytes;
}

/**
 * array_size() - Calculate size of 2-dimensional array.
 *
 * @a: dimension one
 * @b: dimension two
 *
 * Calculates size of 2-dimensional array: @a * @b.
 *
 * Returns: number of bytes needed to represent the array or SIZE_MAX on
 * overflow.
 */
#define array_size(a, b)	size_mul(a, b)

/**
 * array3_size() - Calculate size of 3-dimensional array.
 *
 * @a: dimension one
 * @b: dimension two
 * @c: dimension three
 *
 * Calculates size of 3-dimensional array: @a * @b * @c.
 *
 * Returns: number of bytes needed to represent the array or SIZE_MAX on
 * overflow.
 */
#define array3_size(a, b, c)	size_mul(size_mul(a, b), c)


/*
 * Compute a*b+c, returning SIZE_MAX on overflow. Internal helper for
 * struct_size() below.
 */
static inline __must_check size_t __ab_c_size(size_t a, size_t b, size_t c)
{
	size_t bytes;

	if (check_mul_overflow(a, b, &bytes))
		return SIZE_MAX;
	if (check_add_overflow(bytes, c, &bytes))
		return SIZE_MAX;

	return bytes;
}

/**
 * struct_size() - Calculate size of structure with trailing array.
 * @p: Pointer to the structure.
 * @member: Name of the array member.
 * @count: Number of elements in the array.
 *
 * Calculates size of memory needed for structure @p followed by an
 * array of @count number of @member elements.
 *
 * Return: number of bytes needed or SIZE_MAX on overflow.
 */
#define struct_size(p, member, count)					\
	__ab_c_size(count,						\
		    sizeof(*(p)->member) + __must_be_array((p)->member),\
		    sizeof(*(p)))
            
            
#define flex_array_size(p, member, count)				\
	array_size(count,						\
		    sizeof(*(p)->member) + __must_be_array((p)->member))

#endif /* __LINUX_OVERFLOW_H */
