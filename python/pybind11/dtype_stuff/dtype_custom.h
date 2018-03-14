#pragma once

#include <pybind11/pybind11.h>

#include "ufunc_utility.h"

namespace py = pybind11;

struct dtype_generic {
  py::handle cls;
  int dtype_num{-1};
};

template <typename T>
std::type_index index_of() {
  return std::type_index(typeid(T));
}

auto& cls_map() {
  static std::map<std::type_index, dtype_generic> value;
  return value;
}

template <typename T>
int get_dtype_num() {
  return cls_map().at(index_of<T>()).dtype_num;
}

template <typename T>
static py::handle get_class() {
  return cls_map().at(index_of<T>()).cls;
}

template <typename Class>
struct DTypeObject {
  PyObject_HEAD
  Class value;

  static Class* load_raw(py::handle src) {
    DTypeObject* obj = (DTypeObject*)src.ptr();
    return &obj->value;
  }

  static DTypeObject* alloc_py() {
    auto cls = get_class<Class>();
    PyTypeObject* cls_raw = (PyTypeObject*)cls.ptr();
    return (DTypeObject*)cls_raw->tp_alloc((PyTypeObject*)cls.ptr(), 0);
  }

  static PyObject* tp_new(PyTypeObject* type, PyObject* args, PyObject* kwds) {
    return (PyObject*)alloc_py();
  }
};

// Value.
template <typename Class>
struct dtype_arg {
  using ClassObject = DTypeObject<Class>;

  dtype_arg() = default;
  dtype_arg(py::handle h)
    : h_(h) {}
  dtype_arg(const Class& obj)
    : value_(obj) {}
  // Cannot pass pointers in, as they are not registered.

  // Blech. Would love to hide this, but can't.
  bool load(bool convert) {
    auto cls = get_class<Class>();
    if (!py::isinstance(*h_, cls)) {
      if (convert) {
        throw std::runtime_error("Not yet implemented");
      } else {
        throw py::cast_error("Must be of the same type");
      }
    } else {
      ptr_ = ClassObject::load_raw(*h_);
      // Store temporary, because pybind will do weird things.
      value_ = **ptr_;
      return true;
    }
  }

  Class& value() {
    // Reference should stay alive with caster.
    return *value_;
  }

  Class* ptr() const {
    return *ptr_;
  }

  py::object py() const {
    py::handle out;
    if (!h_) {
      ClassObject* obj = ClassObject::alloc_py();
      obj->value = *value_;
      out = (PyObject*)obj;
    } else {
      out = *h_;
    }
    return py::reinterpret_borrow<py::object>(out);
  }
 private:
  optional<py::handle> h_;
  optional<Class*> ptr_;
  optional<Class> value_;
  bool convert_{};
};

template <typename Class>
struct dtype_caster {
  static constexpr auto name = py::detail::_<Class>();
  // Using this structure because `intrinsic_t` masks our abilities to natively
  // use pointers when using `make_caster` with the temporary return check.
  // See fork, modded func `cast_is_known_safe`.
  using Arg = dtype_arg<Class>;
  using ClassObject = DTypeObject<Class>;
  static py::handle cast(Arg& src, py::return_value_policy, py::handle) {
    cerr << "cast\n";
    return Arg(src).py().release();
  }
  bool load(py::handle src, bool convert) {
    cerr << "load\n";
    arg_ = src;
    return arg_.load(convert);
  }
  // Copy `type_caster_base`.
  template <typename T_> using cast_op_type =
      pybind11::detail::cast_op_type<T_>;

  // Er... Is it possible to return a value here?
  operator Class&() { return arg_.value(); }
  // ... Not sure how to enforce copying, without `const`.
  operator Class*() { return arg_.ptr(); }
  Arg arg_;
};

// Following `pybind11/detail/init.h`
template <typename... Args>
struct dtype_init_factory {
  template <typename PyClass, typename... Extra>
  void execute(PyClass& cl, const Extra&... extra) {
    using Class = typename PyClass::Class;
    using ClassObject = typename PyClass::ClassObject;
    // Do not construct this with the name `__init__` as pybind will try to
    // take over the init setup.
    cl.def("_dtype_init", [](Class* self, Args... args) {
      // Old-style. No factories for now.
      new (self) Class(std::forward<Args>(args)...);
    });
    py::dict d = cl.attr("__dict__");
    if (!d.contains("__init__")) {
      py::handle h = cl;
      auto init = cl.attr("_dtype_init");
      auto func = py::cpp_function(
          [init](py::handle self, py::args args, py::kwargs kwargs) {
            // Dispatch.
            self.attr("_dtype_init")(*args, **kwargs);
          }, py::is_method(h));
      cl.attr("__init__") = func;
    }
  }
};

template <typename... Args>
dtype_init_factory<Args...> dtype_init() { return {}; }

template <typename Class_>
class dtype_class : public py::class_<Class_> {
 public:
  using Base = py::class_<Class_>;
  using Class = Class_;
  using ClassObject = DTypeObject<Class>;
  // https://stackoverflow.com/a/12505371/7829525

  dtype_class(py::handle scope, const char* name) : Base(py::none()) {
    register_type(name);

    scope.attr(name) = self();
    auto& entry = cls_map()[index_of<Class>()];
    entry.cls = self();

    // Registry numpy type.
    // (Note that not registering the type will result in infinte recursion).
    entry.dtype_num = register_numpy();
  }

  ~dtype_class() {
    check();    
  }

  template <typename ... Args, typename... Extra>
  dtype_class &def_dtype(dtype_init_factory<Args...>&& init, const Extra&... extra) {
    std::move(init).execute(*this, extra...);
    return *this;
  }

 private:
  py::object& self() { return *this; }
  const py::object& self() const { return *this; }

  void check() const {
    // This `dict` should indicate whether we've directly overridden methods.
    py::dict d = self().attr("__dict__");
    // Without these, numpy goes into infinite recursion. Haven't bothered to
    // figure out exactly why.
    if (!d.contains("__repr__")) {
      throw std::runtime_error("Class is missing explicit __repr__");
    }
    if (!d.contains("__str__")) {
      throw std::runtime_error("Class is missing explicit __str__");
    }
  }

  void register_type(const char* name) {
    auto heap_type = (PyHeapTypeObject*)PyType_Type.tp_alloc(&PyType_Type, 0);
    PY_ASSERT_EX(heap_type, "yar");
    heap_type->ht_name = py::str(name).release().ptr();
    // It's painful to inherit from `np.generic`, because it has no `tp_new`.
    auto& ClassObject_Type = heap_type->ht_type;
    ClassObject_Type.tp_base = &PyGenericArrType_Type;
    // Define other things.
    ClassObject_Type.tp_repr = [](PyObject*) -> PyObject* {
      std::cerr << "SLOT NOT GET CALLED\n";
      exit(100);
    };
    ClassObject_Type.tp_new = &ClassObject::tp_new;
    ClassObject_Type.tp_name = name;  // Er... scope?
    ClassObject_Type.tp_basicsize = sizeof(ClassObject);
    ClassObject_Type.tp_getset = 0;
    // ClassObject_Type.tp_getattro = PyObject_GenericGetAttr;
    // ClassObject_Type.tp_setattro = PyObject_GenericSetAttr;
    ClassObject_Type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HEAPTYPE;
    // ClassObject_Type.tp_dictoffset = offsetof(ClassObject, dict);
    ClassObject_Type.tp_doc = "Stuff.";
    PY_ASSERT_EX(PyType_Ready(&ClassObject_Type) == 0, "");
    self() = py::reinterpret_borrow<py::object>(py::handle((PyObject*)&ClassObject_Type));
  }

  int register_numpy() {
    // Adapted from `test_rational`.
    auto type = (PyTypeObject*)self().ptr();
    typedef struct { char c; Class r; } align_test;
    static PyArray_ArrFuncs arrfuncs;
    static PyArray_Descr descr = {
        PyObject_HEAD_INIT(0)
        type,                   /* typeobj */
        'V',                    /* kind (V = arbitrary) */
        'r',                    /* type */
        '=',                    /* byteorder */
        NPY_NEEDS_PYAPI | NPY_USE_GETITEM | NPY_USE_SETITEM, /* flags */
        0,                      /* type_num */
        sizeof(Class),          /* elsize */
        offsetof(align_test,r), /* alignment */
        0,                      /* subarray */
        0,                      /* fields */
        0,                      /* names */
        &arrfuncs,  /* f */
    };

    PyArray_InitArrFuncs(&arrfuncs);
    // https://docs.scipy.org/doc/numpy/reference/c-api.types-and-structures.html
    arrfuncs.getitem = [](void* in, void* arr) -> PyObject* {
        auto item = (const Class*)in;
        dtype_arg<Class> arg(*item);
        return arg.py().release().ptr();
    };
    arrfuncs.setitem = [](PyObject* in, void* out, void* arr) {
        dtype_arg<Class> arg(in);
        PY_ASSERT_EX(arg.load(true), "Could not convert");
        *(Class*)out = arg.value();
        return 0;
    };
    arrfuncs.copyswap = [](void* dst, void* src, int swap, void* arr) {
        // TODO(eric.cousineau): Figure out actual purpose of this.
        if (!src) return;
        Class* r_dst = (Class*)dst;
        Class* r_src = (Class*)src;
        if (swap) {
            std::swap(*r_dst, *r_src);
        } else {
            *r_dst = *r_src;
        }
    };
    // - Test and ensure this doesn't overwrite our `equal` unfunc.
    arrfuncs.compare = [](const void* d1, const void* d2, void* arr) {
      return 0;
    };
    arrfuncs.fill = [](void* data_, npy_intp length, void* arr) {
      Class* data = (Class*)data_;
      Class delta = data[1] - data[0];
      Class r = data[1];
      npy_intp i;
      for (i = 2; i < length; i++) {
          r += delta;
          data[i] = r;
      }
      return 0;
    };
    arrfuncs.fillwithscalar = [](
            void* buffer_raw, npy_intp length, void* value_raw, void* arr) {
        const Class* value = (const Class*)value_raw;
        Class* buffer = (Class*)buffer_raw;
        for (int k = 0; k < length; k++) {
            buffer[k] = *value;
        }
        return 0;
    };
    Py_TYPE(&descr) = &PyArrayDescr_Type;
    int dtype_num = PyArray_RegisterDataType(&descr);
    self().attr("dtype") =
        py::reinterpret_borrow<py::object>(py::handle((PyObject*)&descr));
    return dtype_num;
  }
};

template <typename Class>
struct npy_format_descriptor_custom {
    static pybind11::dtype dtype() {
        int dtype_num = get_dtype_num<Class>();
        if (auto ptr = py::detail::npy_api::get().PyArray_DescrFromType_(dtype_num))
            return py::reinterpret_borrow<pybind11::dtype>(ptr);
        py::pybind11_fail("Unsupported buffer format!");
    }
};

namespace pybind11 { namespace detail {

template <typename T>
struct cast_is_known_safe<T,
    enable_if_t<std::is_base_of<
        dtype_caster<intrinsic_t<T>>, make_caster<T>>::value>>
    : public std::true_type {};

template <>
struct npy_format_descriptor<py::object> {
    static pybind11::dtype dtype() {
        if (auto ptr = npy_api::get().PyArray_DescrFromType_(NPY_OBJECT))
            return reinterpret_borrow<pybind11::dtype>(ptr);
        pybind11_fail("Unsupported buffer format!");
    }
};

} } // namespace pybind11 { namespace detail {
