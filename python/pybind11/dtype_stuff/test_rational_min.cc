/*
 *****************************************************************************
 **       This file was autogenerated from a template  DO NOT EDIT!!!!      **
 **       Changes should be made to the original source (.src) file         **
 *****************************************************************************
 */

/* Fixed size rational numbers exposed to Python */

#define NPY_NO_DEPRECATED_API NPY_API_VERSION

#include <Python.h>
#include <structmember.h>
#include <numpy/arrayobject.h>
#include <numpy/ufuncobject.h>
#include <numpy/npy_3kcompat.h>
#include <math.h>

#include <pybind11/pybind11.h>
#include <pybind11/eval.h>

namespace py = pybind11;

/* Relevant arithmetic exceptions */

/* Fixed precision rational numbers */

typedef struct {
    /* numerator */
    npy_int32 n{1};
    /*
     * denominator minus one: numpy.zeros() uses memset(0) for non-object
     * types, so need to ensure that rational(0) has all zero bytes
     */
    npy_int32 dmm{1};
} rational;

/* Numpy support */

static PyObject*
npyrational_getitem(void* data, void* arr) {
    rational r;
    memcpy(&r,data,sizeof(rational));
    return py::globals()["rational_wrap"](py::cast<rational>(r)).release().ptr();
}

static int
npyrational_setitem(PyObject* item, void* data, void* arr) {
    rational r{};
    memcpy(data,&r,sizeof(rational));
    return 0;
}

static NPY_INLINE void
byteswap(npy_int32* x) {
    char* p = (char*)x;
    size_t i;
    for (i = 0; i < sizeof(*x)/2; i++) {
        size_t j = sizeof(*x)-1-i;
        char t = p[i];
        p[i] = p[j];
        p[j] = t;
    }
}

static void
npyrational_copyswap(void* dst, void* src, int swap, void* arr) {
    rational* r;
    if (!src) {
        return;
    }
    r = (rational*)dst;
    memcpy(r,src,sizeof(rational));
    if (swap) {
        byteswap(&r->n);
        byteswap(&r->dmm);
    }
}

static PyArray_ArrFuncs npyrational_arrfuncs;

typedef struct { char c; rational r; } align_test;

PyArray_Descr npyrational_descr = {
    PyObject_HEAD_INIT(0)
    nullptr,       /* typeobj */
    'V',                    /* kind */
    'r',                    /* type */
    '=',                    /* byteorder */
    /*
     * For now, we need NPY_NEEDS_PYAPI in order to make numpy detect our
     * exceptions.  This isn't technically necessary,
     * since we're careful about thread safety, and hopefully future
     * versions of numpy will recognize that.
     */
    NPY_NEEDS_PYAPI | NPY_USE_GETITEM | NPY_USE_SETITEM, /* hasobject */
    0,                      /* type_num */
    sizeof(rational),       /* elsize */
    offsetof(align_test,r), /* alignment */
    0,                      /* subarray */
    0,                      /* fields */
    0,                      /* names */
    &npyrational_arrfuncs,  /* f */
};

#define RETVAL
PyMODINIT_FUNC inittest_rational(void) {

    PyObject *m = NULL;
    PyObject* numpy_str;
    PyObject* numpy;
    int npy_rational;

    import_array();
    import_umath();
    numpy_str = PyUString_FromString("numpy");
    numpy = PyImport_Import(numpy_str);
    Py_DECREF(numpy_str);

    /* Can't set this until we import numpy */
    // HACK(eric)
    py::globals()["np"] = py::module::import("numpy");
    py::module mp("test_rational");
    py::globals()["m"] = mp;
    py::class_<rational> cls(mp, "rational");
    cls.def(py::init());

    py::exec(R"""(
class rational_wrap():
    def __init__(self):
        self.r = m.rational()
    def __str__(self):
        return self.r.__str__()
m.rational_wrap = rational_wrap
)""");
    py::object new_cls = mp.attr("rational_wrap");

    PyTypeObject& PyRational_Type = *(PyTypeObject*)new_cls.ptr();
    PyRational_Type.tp_base = &PyGenericArrType_Type;  // np.generic

    /* Initialize rational type object */
    assert(PyType_Ready(&PyRational_Type) >= 0);

    /* Initialize rational descriptor */
    PyArray_InitArrFuncs(&npyrational_arrfuncs);
    npyrational_arrfuncs.getitem = npyrational_getitem;
    npyrational_arrfuncs.setitem = npyrational_setitem;
    npyrational_arrfuncs.copyswap = npyrational_copyswap;

    npyrational_descr.typeobj = &PyRational_Type;
    Py_TYPE(&npyrational_descr) = &PyArrayDescr_Type;
    npy_rational = PyArray_RegisterDataType(&npyrational_descr);
    assert(npy_rational>=0);

    /* Support dtype(rational) syntax */
    // NOTE: This just produces ints, rather than rational objects...
    assert(PyDict_SetItemString(PyRational_Type.tp_dict, "dtype",
                             (PyObject*)&npyrational_descr) >= 0);
    m = mp.ptr();

    return RETVAL;
}
