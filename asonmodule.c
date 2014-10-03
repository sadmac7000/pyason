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
#include "ason/ason.h"
#include "ason/print.h"
#include "ason/read.h"

/**
 * ASON value object.
 **/
typedef struct {
	PyObject_HEAD
	ason_t *value;
} Ason;

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
 * Convert an Ason object to string.
 **/
static PyObject *
Ason_str(Ason *self)
{
	char *data = ason_asprint_unicode(self->value);
	PyObject *ret = Py_BuildValue("s", data);
	free(data);
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
static PyObject * Ason_complement(Ason *self);

/**
 * Method table for ASON value object.
 **/
static PyMethodDef Ason_methods[] = {
	{NULL}
};

static PyNumberMethods ason_AsonNumber = {
	.nb_or = (binaryfunc)Ason_union,
	.nb_and = (binaryfunc)Ason_intersect,
	.nb_invert = (unaryfunc)Ason_complement,
};

static int Ason_init(Ason *self, PyObject *args, PyObject *kwds);

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
	0,
	0,
	0,
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
	size_t i;


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
		obj = PyDict_Items(obj);
		size = PyList_Size(obj);

		ret = ason_read("{}");

		for (i = 0; i < size; i++) {
			item = PyList_GetItem(obj, i);

			if (! item)
				return NULL;

			key = PyTuple_GetItem(item, 0);

			if (! key)
				return NULL;

			item = PyTuple_GetItem(item, 1);

			if (! item)
				return NULL;

			if (! PyUnicode_Check(key)) {
				PyErr_Format(PyExc_TypeError,
			     "Cannot ASONify dict with non-string keys");
				return NULL;
			}

			tmp1 = pyobject_to_ason(item);

			if (! tmp1)
				return NULL;

			str_key = PyUnicode_AsUTF8(key);
			if (! key)
				return NULL;

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
 * Perform an Ason operation.
 **/
static PyObject *
Ason_operate(Ason *self, Ason *other, const char *fmt)
{
	Ason *ret;
	PyObject *tmp;

	if (! PyObject_TypeCheck(other, &ason_AsonType)) {
		tmp = Py_BuildValue("(O)", other);
		if (! tmp)
			return NULL;

		other = PyObject_New(Ason, &ason_AsonType);
		Py_DECREF(tmp);
		if (! other)
			return NULL;

		if (Ason_init(other, tmp, NULL) < 0)
			return NULL;
	}

	ret = PyObject_New(Ason, &ason_AsonType);
	ret->value = ason_read(fmt, self->value, other->value);
	return (PyObject *)ret;
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
 * The ason module itself.
 **/
static PyModuleDef asonmodule = {
	PyModuleDef_HEAD_INIT,
	"ason",
	"Module for manipulating ASON values.",
	-1,
	NULL, NULL, NULL, NULL, NULL
};

/**
 * Initialization for the ason module.
 **/
PyMODINIT_FUNC
PyInit_ason(void)
{
	PyObject *m;

	if (PyType_Ready(&ason_AsonType) < 0)
		return NULL;

	m = PyModule_Create(&asonmodule);
	if (m == NULL)
		return NULL;

	Py_INCREF(&ason_AsonType);
	PyModule_AddObject(m, "ason", (PyObject *)&ason_AsonType);
	return m;
}
