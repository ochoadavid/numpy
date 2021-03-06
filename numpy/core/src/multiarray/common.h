#ifndef _NPY_PRIVATE_COMMON_H_
#define _NPY_PRIVATE_COMMON_H_
#include <numpy/npy_common.h>
#include <numpy/npy_cpu.h>

#define error_converting(x)  (((x) == -1) && PyErr_Occurred())

/*
 * Recursively examines the object to determine an appropriate dtype
 * to use for converting to an ndarray.
 *
 * 'obj' is the object to be converted to an ndarray.
 *
 * 'maxdims' is the maximum recursion depth.
 *
 * 'out_dtype' should be either NULL or a minimal starting dtype when
 * the function is called. It is updated with the results of type
 * promotion. This dtype does not get updated when processing NA objects.
 *
 * Returns 0 on success, -1 on failure.
 */
NPY_NO_EXPORT int
PyArray_DTypeFromObject(PyObject *obj, int maxdims,
                        PyArray_Descr **out_dtype);

NPY_NO_EXPORT int
PyArray_DTypeFromObjectHelper(PyObject *obj, int maxdims,
                              PyArray_Descr **out_dtype, int string_status);

NPY_NO_EXPORT PyObject *
PyArray_GetAttrString_SuppressException(PyObject *v, char *name);

/*
 * Returns NULL without setting an exception if no scalar is matched, a
 * new dtype reference otherwise.
 */
NPY_NO_EXPORT PyArray_Descr *
_array_find_python_scalar_type(PyObject *op);

NPY_NO_EXPORT PyArray_Descr *
_array_typedescr_fromstr(char *str);

NPY_NO_EXPORT char *
index2ptr(PyArrayObject *mp, npy_intp i);

NPY_NO_EXPORT int
_zerofill(PyArrayObject *ret);

NPY_NO_EXPORT int
_IsAligned(PyArrayObject *ap);

NPY_NO_EXPORT npy_bool
_IsWriteable(PyArrayObject *ap);

NPY_NO_EXPORT void
offset_bounds_from_strides(const int itemsize, const int nd,
                           const npy_intp *dims, const npy_intp *strides,
                           npy_intp *lower_offset, npy_intp *upper_offset);


/*
 * Returns -1 and sets an exception if *index is an invalid index for
 * an array of size max_item, otherwise adjusts it in place to be
 * 0 <= *index < max_item, and returns 0.
 * 'axis' should be the array axis that is being indexed over, if known. If
 * unknown, use -1.
 */
static NPY_INLINE int
check_and_adjust_index(npy_intp *index, npy_intp max_item, int axis)
{
    /* Check that index is valid, taking into account negative indices */
    if ((*index < -max_item) || (*index >= max_item)) {
        /* Try to be as clear as possible about what went wrong. */
        if (axis >= 0) {
            PyErr_Format(PyExc_IndexError,
                         "index %"NPY_INTP_FMT" is out of bounds "
                         "for axis %d with size %"NPY_INTP_FMT,
                         *index, axis, max_item);
        } else {
            PyErr_Format(PyExc_IndexError,
                         "index %"NPY_INTP_FMT" is out of bounds "
                         "for size %"NPY_INTP_FMT,
                         *index, max_item);
        }
        return -1;
    }
    /* adjust negative indices */
    if (*index < 0) {
        *index += max_item;
    }
    return 0;
}


/*
 * return true if pointer is aligned to 'alignment'
 */
static NPY_INLINE int
npy_is_aligned(const void * p, const npy_uintp alignment)
{
    /*
     * alignment is usually a power of two
     * the test is faster than a direct modulo
     */
    if (NPY_LIKELY((alignment & (alignment - 1)) == 0)) {
        return ((npy_uintp)(p) & ((alignment) - 1)) == 0;
    }
    else {
        return ((npy_uintp)(p) % alignment) == 0;
    }
}

/*
 * memchr with stride and invert argument
 * intended for small searches where a call out to libc memchr is costly.
 * stride must be a multiple of size.
 * compared to memchr it returns one stride past end instead of NULL if needle
 * is not found.
 */
static NPY_INLINE char *
npy_memchr(char * haystack, char needle,
           npy_intp stride, npy_intp size, npy_intp * subloopsize, int invert)
{
    char * p = haystack;
    char * const end = haystack + size;
    if (stride == 0) {
        if (!invert) {
            p = (*p != needle) ? end : haystack;
        }
        else {
            p = (*p == needle) ? end : haystack;
        }
        *subloopsize = (p - haystack);
        return haystack;
    }

    if (!invert) {
        /*
         * this is usually the path to determine elements to process,
         * performance less important here.
         * memchr has large setup cost if 0 byte is close to start.
         */
        while (p < end && *p != needle) {
            p += stride;
        }
    }
    else {
        /* usually find elements to skip path */
#if (defined HAVE___BUILTIN_CTZ && defined NPY_CPU_HAVE_UNALIGNED_ACCESS)
        if (needle == 0 && stride == 1) {
            while (p < end - (size % sizeof(unsigned int))) {
                unsigned int  v = *(unsigned int*)p;
                if (v == 0) {
                    p += sizeof(unsigned int);
                    continue;
                }
                p += __builtin_ctz(v) / 8;
                *subloopsize = (p - haystack) / stride;
                return p;
            }
        }
#endif
        while (p < end && *p == needle) {
            p += stride;
        }
    }

    /* division is very expensive */
    if (NPY_LIKELY(stride == 1)) {
        *subloopsize = (p - haystack);
    }
    else {
        *subloopsize = (p - haystack) / stride;
    }
    return p;
}

#include "ucsnarrow.h"

#endif
