/**
 * Copyright © 2013, 2014 Red Hat, Casey Dahlin <casey.dahlin@gmail.com>
 *
 * This file is part of pyason.
 *
 * pyason is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pyason is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with pyason. If not, see <http://www.gnu.org/licenses/>.
 **/

#include <Python.h>
#include <string.h>
#include <ason/ason.h>
#include <ason/print.h>
#include <ason/read.h>
#include <ason/namespace.h>
#include <ason/iter.h>

/**
 * ASON value object.
 **/
typedef struct {
	PyObject_HEAD
	ason_t *value;
} Ason;

/**
 * Ason iterator object
 **/
typedef struct {
	PyObject_HEAD
	ason_iter_t *iter;
	int entered;
	int in_object;
} AsonIter;

/**
 * Destroy an Ason python object.
 **/
static void
Ason_dealloc(Ason *self)
{
	ason_destroy(self->value);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

/**
 * Destroy an AsonIter python object.
 **/
static void
AsonIter_dealloc(AsonIter *self)
{
	ason_iter_destroy(self->iter);
	Py_TYPE(self)->tp_free((PyObject *)self);
}

/**
 * Allocate an Ason object.
 **/
static PyObject *
Ason_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	Ason *self;

	self = (Ason *)type->tp_alloc(type, 0);
	if (self == NULL)
		return NULL;

	self->value = ASON_EMPTY;

	return (PyObject *)self;
}

/**
 * Allocate an AsonIter object.
 **/
static PyObject *
AsonIter_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	AsonIter *self;

	self = (AsonIter *)type->tp_alloc(type, 0);
	if (self == NULL)
		return NULL;

	self->iter = NULL;
	self->entered = 0;
	self->in_object = 0;

	return (PyObject *)self;
}

/**
 * Convert an Ason object to string.
 **/
static PyObject *
Ason_str(Ason *self)
{
	char *data;
	long long ldata;
	double ddata;
	PyObject *ret;

	switch (ason_type(self->value)) {
	case ASON_TYPE_STRING:
		data = ason_string(self->value);
		ret = Py_BuildValue("s", data);
		free(data);
		break;
	case ASON_TYPE_NUMERIC:
		ddata = ason_double(self->value);
		ldata = ason_long(self->value);

		if (ddata != ldata)
			ret = Py_BuildValue("d", ddata);
		else
			ret = Py_BuildValue("L", ldata);
		break;
	default:
		data = ason_asprint_unicode(self->value);
		ret = Py_BuildValue("s", data);
		free(data);
	};

	return ret;
}

/**
 * Programmatic representation of an Ason object
 **/
static PyObject *
Ason_repr(Ason *self)
{
	char *data = ason_asprint_unicode(self->value);
	char *repr;

	asprintf(&repr, "ason(%s)", data);
	PyObject *ret = Py_BuildValue("s", repr);
	free(data);
	free(repr);
	return ret;
}

static PyObject * Ason_intersect(Ason *self, Ason *other);
static PyObject * Ason_union(Ason *self, Ason *other);
static PyObject * Ason_join(Ason *self, PyObject *args);
static PyObject * Ason_complement(Ason *self);
static PyObject * Ason_compare(PyObject *a, PyObject *b, int op);
static PyObject * AsonIter_next(AsonIter *self);
static PyObject * Ason_int(Ason *self);
static PyObject * Ason_float(Ason *self);

static AsonIter * Ason_iterate(Ason *self);

static int Ason_init(Ason *self, PyObject *args, PyObject *kwds);
static int AsonIter_init(AsonIter *self, PyObject *args, PyObject *kwds);

/**
 * Method table for ASON value object.
 **/
static PyMethodDef Ason_methods[] = {
	{"join", (PyCFunction)Ason_join, METH_VARARGS,
		"Perform an ASON join"},
	{NULL}
};

static PyNumberMethods ason_AsonNumber = {
	.nb_or = (binaryfunc)Ason_union,
	.nb_and = (binaryfunc)Ason_intersect,
	.nb_invert = (unaryfunc)Ason_complement,
	.nb_int = (unaryfunc)Ason_int,
	.nb_float = (unaryfunc)Ason_float,
};

/**
 * Method table for AsonIter object.
 **/
static PyMethodDef AsonIter_methods[] = {
	{NULL}
};

/**
 * Type for ASON value object.
 **/
static PyTypeObject ason_AsonType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ason.Ason",
	sizeof(Ason),
	0,
	(destructor)Ason_dealloc,
	0,
	0,
	0,
	0,
	(reprfunc)Ason_repr,
	&ason_AsonNumber,
	0,
	0,
	0,
	0,
	(reprfunc)Ason_str,
	0,
	0,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	"An ASON value",
	0,
	0,
	(richcmpfunc)Ason_compare,
	0,
	(getiterfunc)Ason_iterate,
	0,
	Ason_methods,
	0, /* members */
	0,
	0,
	0,
	0,
	0,
	0,
	(initproc)Ason_init,
	0,
	Ason_new
};

/**
 * Type for AsonIter object.
 **/
static PyTypeObject ason_AsonIterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"ason.AsonIter",
	sizeof(AsonIter),
	0,
	(destructor)AsonIter_dealloc,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	"An iterator for ASON values",
	0,
	0,
	0,
	0,
	0,
	(iternextfunc)AsonIter_next,
	AsonIter_methods,
	0, /* members */
	0,
	0,
	0,
	0,
	0,
	0,
	(initproc)AsonIter_init,
	0,
	AsonIter_new
};

/**
 * Convert a python value to an ASON value.
 **/
static ason_t *
pyobject_to_ason(PyObject *obj)
{
	PyObject *item;
	PyObject *key;
	int64_t ival;
	uint64_t uval;
	double dval;
	ssize_t size;
	char *list_data;
	char *str_key;
	ason_t *tmp1;
	ason_t *tmp2;
	ason_t *ret;
	Py_ssize_t i;


	if (PyUnicode_Check(obj)) {
		str_key = PyUnicode_AsUTF8(obj);

		if (! str_key)
			return NULL;

		return ason_read("?s", str_key);
	}
retry:
	if (PyBool_Check(obj)) {
		if (obj == Py_False)
			return  ASON_FALSE;
		else
			return  ASON_TRUE;
	}

	if (obj == Py_None)
		return ASON_NULL;

	if (PyLong_Check(obj)) {
		ival = PyLong_AsLongLong(obj);

		if (! PyErr_Occurred())
			return ason_read("?I", ival);

		PyErr_Clear();
		uval = PyLong_AsUnsignedLongLong(obj);

		if (PyErr_Occurred())
			return NULL;

		return ason_read("?U", uval);
	}

	if (PyFloat_Check(obj)) {
		dval = PyFloat_AsDouble(obj);

		if (PyErr_Occurred())
			return NULL;

		return ason_read("?F", dval);
	}

	if (PyObject_TypeCheck(obj, &ason_AsonType))
		return ason_copy(((Ason *)obj)->value);

	if (PyList_Check(obj)) {
		size = PyList_Size(obj);

		if (size < 0)
			return NULL;

		if (size == 0)
			return ason_read("[]");

		list_data = malloc(4 + size * 2);

		if (! list_data) {
			PyErr_NoMemory();
			return NULL;
		}

		list_data[0] = '?';
		list_data[1] = '&';
		list_data[2] = '[';
		list_data[2 + size * 2] = ']';
		list_data[3 + size * 2] = '\0';

		for (i = 3; i < (size * 2 + 3); i += 2) {
			if (i > 3)
				list_data[i - 1] = ',';
			list_data[i] = 'U';
		}

		ret = ASON_UNIVERSE;

		for (i = 0; i < size; i++) {
			tmp1 = ret;
			list_data[i * 2 + 3] = '?';

			item = PyList_GetItem(obj, i);

			if (! item)
				break;

			tmp2 = pyobject_to_ason(item);

			if (! tmp2)
				break;

			ret = ason_read(list_data, ret, tmp2);
			ason_destroy(tmp1);
			ason_destroy(tmp2);

			list_data[i * 2 + 3] = 'U';
		}

		free(list_data);

		if (i < size) {
			ason_destroy(ret);
			return NULL;
		}

		return ret;
	}

	if (PyDict_Check(obj)) {
		ret = ason_read("{}");

		for (i = 0; PyDict_Next(obj, &i, &key, &item);) {
			if (!key || !item) {
				ason_destroy(ret);
				return NULL;
			}

			if (! PyUnicode_Check(key)) {
				PyErr_Format(PyExc_TypeError,
			     "Cannot ASONify dict with non-string keys");
				ason_destroy(ret);
				return NULL;
			}

			str_key = PyUnicode_AsUTF8(key);
			if (! key) {
				ason_destroy(ret);
				return NULL;
			}

			tmp1 = pyobject_to_ason(item);

			if (! tmp1) {
				ason_destroy(ret);
				return NULL;
			}

			tmp2 = ret;
			ret = ason_read("? : { ?s: ? }", ret, str_key, tmp1);
			ason_destroy(tmp1);
			ason_destroy(tmp2);

			if (! ret)
				return NULL;
		}

		return ret;
	}

	if (PyObject_HasAttrString(obj, "__ason__")) {
		obj = PyObject_GetAttrString(obj, "__ason__");
	} else if (PyObject_HasAttrString(obj, "__json__")) {
		obj = PyObject_GetAttrString(obj, "__json__");
	} else {
		PyErr_Format(PyExc_TypeError, "Type '%s' is not ASONifiable",
			     obj->ob_type->tp_name);
		return NULL;
	}

	if (! obj)
		return NULL;

	obj = PyObject_CallFunctionObjArgs(obj, NULL);

	if (! obj)
		return NULL;

	if (PyUnicode_Check(obj)) {
		str_key = PyUnicode_AsUTF8(obj);

		if (! str_key)
			return NULL;

		return ason_read(str_key);
	}

	goto retry;
}

/**
 * Initialize an Ason object.
 **/
static int
Ason_init(Ason *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;
	static char *kwlist[] = {"value", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &obj))
		return -1;

	self->value = pyobject_to_ason(obj);

	if (! self->value)
		return -1;

	return 0;
}

/**
 * Initialize an AsonIter object.
 **/
static int
AsonIter_init(AsonIter *self, PyObject *args, PyObject *kwds)
{
	PyObject *obj;
	static char *kwlist[] = {"value", NULL};

	if (! PyArg_ParseTupleAndKeywords(args, kwds, "O", kwlist, &obj))
		return -1;

	if (! PyObject_TypeCheck(obj, &ason_AsonType)) {
		PyErr_Format(PyExc_TypeError,
			     "First argument must be of type 'Ason'");
		return -1;
	}

	self->iter = ason_iterate(((Ason *)obj)->value);
	self->entered = 0;

	switch (ason_iter_type(self->iter)) {
	case ASON_TYPE_OBJECT:
	case ASON_TYPE_UOBJECT:
		self->in_object = 1;
		break;
	case ASON_TYPE_LIST:
	case ASON_TYPE_UNION:
		self->in_object = 0;
		break;
	default:
		PyErr_Format(PyExc_TypeError, "ASON value is not iterable");
		return -1;
	};

	return 0;
}

/**
 * Get the next value for an AsonIter.
 **/
static PyObject *
AsonIter_next(AsonIter *self)
{
	Ason *val;
	PyObject *tuple;
	char *str_key;
	int got;

	if (! self->entered) {
		got = ason_iter_enter(self->iter);
		self->entered = 1;
	} else {
		got = ason_iter_next(self->iter);
	}

	if (! got) /* No exception. Weird right? */
		return NULL;

	val = PyObject_New(Ason, &ason_AsonType);

	if (! val)
		return NULL;

	val->value = ason_iter_value(self->iter);

	if (! self->in_object)
		return (PyObject *)val;

	str_key = ason_iter_key(self->iter);

	tuple = Py_BuildValue("(sO)", str_key, val);
	Py_DECREF(val);
	free(str_key);

	return tuple;
}

/**
 * Get an iterator for an Ason object.
 **/
static AsonIter *
Ason_iterate(Ason *self)
{
	AsonIter *ret;
	PyObject *args;
	int got = 0;


	args = Py_BuildValue("(O)", self);

	if (! args)
		return NULL;

	ret = PyObject_New(AsonIter, &ason_AsonIterType);

	if (ret)
		got = AsonIter_init(ret, args, NULL);

	Py_DECREF(args);

	if (! got)
		return ret;

	Py_XDECREF(ret);

	return NULL;
}

/**
 * Perform an Ason operation.
 **/
static PyObject *
Ason_operate(Ason *self, Ason *other, const char *fmt)
{
	Ason *ret;
	PyObject *tmp;

	ret = PyObject_New(Ason, &ason_AsonType);

	if (! ret)
		return NULL;

	if (! PyObject_TypeCheck(other, &ason_AsonType)) {
		tmp = Py_BuildValue("(O)", other);
		if (! tmp) {
			Py_DECREF(ret);
			return NULL;
		}

		other = PyObject_New(Ason, &ason_AsonType);
		if (! other || Ason_init(other, tmp, NULL) < 0) {
			Py_DECREF(ret);
			Py_DECREF(tmp);
			return NULL;
		}

		Py_DECREF(tmp);
	}

	ret->value = ason_read(fmt, self->value, other->value);
	return (PyObject *)ret;
}

/**
 * Make a universal object
 **/
static Ason *
ason_uobject(PyObject *self, PyObject *args, PyObject *kwargs)
{
	PyObject *dict;
	PyObject *tmp = NULL;
	ason_t *object;
	ason_t *ret;
	Ason *ret_object;

	if (! PyArg_ParseTuple(args, "|O", &tmp))
		return NULL;

	dict = PyDict_New();

	if (tmp && !PyDict_Check(tmp)) {
		 if (PyDict_MergeFromSeq2(dict, tmp, 1) < 0)
			return NULL;
	} else if (tmp && PyDict_Update(dict, tmp) < 0) {
		return NULL;
	}

	if (kwargs && PyDict_Update(dict, kwargs) < 0)
		return NULL;

	object = pyobject_to_ason(dict);
	Py_DECREF(dict);

	if (! object)
		return NULL;

	ret = ason_read("? : {*}", object);
	ason_destroy(object);

	ret_object = PyObject_New(Ason, &ason_AsonType);

	if (! ret_object) {
		ason_destroy(ret);
		return NULL;
	}

	ret_object->value = ret;

	return ret_object;
}

/**
 * Parse an ASON string.
 **/
static PyObject *
ason_parse(PyObject *self, PyObject *args, PyObject *kwargs)
{
	char *string;
	Ason *ret;
	ason_ns_t *ns = NULL;
	PyObject *item;
	PyObject *key;
	char *str_key;
	Py_ssize_t i;
	ason_t *ason_item;


	if (! PyArg_ParseTuple(args, "s", &string))
		return NULL;

	if (! kwargs)
		goto do_parse;

	ns = ason_ns_create(ASON_NS_RAM, NULL);
	if (! ns) {
		PyErr_Format(PyExc_RuntimeError,
			     "Could not create ASON namespace");
		return NULL;
	}

	for (i = 0; PyDict_Next(kwargs, &i, &key, &item);) {
		if (!key || !item)
			goto kill_namespace;

		if (! PyUnicode_Check(key)) {
			PyErr_Format(PyExc_TypeError, "Bad keyword list");
			goto kill_namespace;
		}

		str_key = PyUnicode_AsUTF8(key);

		if (! str_key)
			goto kill_namespace;

		ason_item = pyobject_to_ason(item);
		if (ason_ns_mkvar(ns, str_key)) {
			PyErr_Format(PyExc_RuntimeError,
				     "mkvar error from ASON namespace");
			goto kill_namespace;
		}

		if (ason_ns_store(ns, str_key, ason_item)) {
			PyErr_Format(PyExc_RuntimeError,
				     "store error from ASON namespace");
			goto kill_namespace;
		}
	}

do_parse:
	ret = PyObject_New(Ason, &ason_AsonType);

	if (! ret)
		goto kill_namespace;

	ret->value = ason_ns_read(ns, string);

	if (ret->value)
		return (PyObject *)ret;

	Py_DECREF(ret);
	PyErr_Format(PyExc_TypeError, "Could not parse ASON expression");

kill_namespace:
	ason_ns_destroy(ns);
	return NULL;
}

/**
 * Convert an Ason object to a float
 **/
static PyObject *
Ason_float(Ason *self)
{
	if (ason_type(self->value) == ASON_TYPE_NUMERIC)
		return Py_BuildValue("d", ason_double(self->value));

	PyErr_Format(PyExc_TypeError, "ASON expression must be numeric");
	return NULL;
}

/**
 * Convert an Ason object to an int
 **/
static PyObject *
Ason_int(Ason *self)
{
	if (ason_type(self->value) == ASON_TYPE_NUMERIC)
		return Py_BuildValue("L", ason_long(self->value));

	PyErr_Format(PyExc_TypeError, "ASON expression must be numeric");
	return NULL;
}

/**
 * Compare two values, one of which is an Ason value.
 **/
static PyObject *
Ason_compare(PyObject *a, PyObject *b, int op)
{
	Ason *self = (Ason *)a;
	ason_t *other;
	int cmp;
	char *str_a;
	char *str_b;
	double dbl_a;
	double dbl_b;

	if (! PyObject_TypeCheck(self, &ason_AsonType)) {
		self = (Ason *)b;
		if (! PyObject_TypeCheck(self, &ason_AsonType))
			PyErr_Format(PyExc_TypeError,
				     "Ason comparator called on non-Ason value");

		if (op == Py_LT)
			op = Py_GT;
		if (op == Py_GT)
			op = Py_LT;
		if (op == Py_LE)
			op = Py_GE;
		if (op == Py_GE)
			op = Py_LE;

		other = pyobject_to_ason(a);
	} else {
		other = pyobject_to_ason(b);
	}

	if (PyErr_Occurred()) {
		PyErr_Clear();

		if (op == Py_NE)
			goto ret_true;
		if (op == Py_EQ)
			goto ret_false;

		PyErr_Format(PyExc_TypeError, "Type cannot be compared "
			     "to Ason value");
		return NULL;
	}

	if (ason_check_equal(other, self->value)) {
		if (op == Py_EQ)
			goto ret_true;
		else if (op == Py_NE)
			goto ret_false;
		else if (op == Py_GE)
			goto ret_true;
		else if (op == Py_LE)
			goto ret_true;
	} else {
		if (op == Py_EQ)
			goto ret_false;
		else if (op == Py_NE)
			goto ret_true;
		else if (op == Py_GE)
			op = Py_GT;
		else if (op == Py_LE)
			op = Py_LT;
	}

	if (ason_type(self->value) == ASON_TYPE_STRING &&
	    ason_type(other) == ASON_TYPE_STRING) {
		str_a = ason_string(self->value);
		str_b = ason_string(other);
		cmp = strcmp(str_a, str_b);
		free(str_a);
		free(str_b);

		if (cmp < 0 && op == Py_LT)
			goto ret_true;
		else
			goto ret_false;
	}

	if (ason_type(self->value) == ASON_TYPE_NUMERIC &&
	    ason_type(other) == ASON_TYPE_NUMERIC) {
		dbl_a = ason_double(self->value);
		dbl_b = ason_double(other);

		if (dbl_a < dbl_b && op == Py_LT)
			goto ret_true;
		else
			goto ret_false;
	}

	if (op == Py_LT && ason_check_represented_in(self->value, other))
		goto ret_true;
	else if (op == Py_LT)
		goto ret_false;
	else if (ason_check_represented_in(other, self->value))
		goto ret_true;
	else
		goto ret_false;

ret_true:
	ason_destroy(other);
	return Py_True;
ret_false:
	ason_destroy(other);
	return Py_False;
}

/**
 * Intersect two Ason objects.
 **/
static PyObject *
Ason_intersect(Ason *self, Ason *other)
{
	return Ason_operate(self, other, "? & ?");
}

/**
 * Join two Ason objects.
 **/
static PyObject *
Ason_join(Ason *self, PyObject *args)
{
	Ason *other;

	if (! PyArg_ParseTuple(args, "O", &other))
		return NULL;

	return Ason_operate(self, other, "? : ?");
}

/**
 * Union two Ason objects.
 **/
static PyObject *
Ason_union(Ason *self, Ason *other)
{
	return Ason_operate(self, other, "? | ?");
}

/**
 * Complement an Ason object.
 **/
static PyObject *
Ason_complement(Ason *self)
{
	Ason *ret;

	ret = PyObject_New(Ason, &ason_AsonType);
	ret->value = ason_read("!?", self->value);
	return (PyObject *)ret;
}

/**
 * Methods for the ason module.
 **/
static PyMethodDef asonmodule_methods[] = {
	{"parse", (PyCFunction)ason_parse, METH_VARARGS | METH_KEYWORDS,
		"Parse a string as an ASON value"},
	{"uobject", (PyCFunction)ason_uobject, METH_VARARGS | METH_KEYWORDS,
		"Parse a string as an ASON value"},
	{NULL}
};

/**
 * The ason module itself.
 **/
static PyModuleDef asonmodule = {
	PyModuleDef_HEAD_INIT,
	"ason",
	"Module for manipulating ASON values.",
	-1,
	asonmodule_methods
};

/**
 * Initialization for the ason module.
 **/
PyMODINIT_FUNC
PyInit_ason(void)
{
	PyObject *m;
	Ason *empty;
	Ason *universe;
	Ason *wild;

	if (PyType_Ready(&ason_AsonType) < 0)
		return NULL;

	if (PyType_Ready(&ason_AsonIterType) < 0)
		return NULL;

	m = PyModule_Create(&asonmodule);
	if (m == NULL)
		return NULL;

	empty = PyObject_New(Ason, &ason_AsonType);
	if (! empty)
		goto fail_empty;

	universe = PyObject_New(Ason, &ason_AsonType);
	if (! universe)
		goto fail_universe;

	wild = PyObject_New(Ason, &ason_AsonType);
	if (! wild)
		goto fail_wild;

	((Ason *)universe)->value = ASON_UNIVERSE;
	((Ason *)wild)->value = ASON_WILD;
	((Ason *)empty)->value = ASON_EMPTY;

	Py_INCREF(&ason_AsonType);
	Py_INCREF(&ason_AsonIterType);

	PyModule_AddObject(m, "ason", (PyObject *)&ason_AsonType);
	PyModule_AddObject(m, "U", (PyObject *)universe);
	PyModule_AddObject(m, "WILD", (PyObject *)wild);
	PyModule_AddObject(m, "EMPTY", (PyObject *)empty);

	return m;

fail_wild:
	Py_DECREF(universe);
fail_universe:
	Py_DECREF(empty);
fail_empty:
	Py_DECREF(m);

	return NULL;
}
