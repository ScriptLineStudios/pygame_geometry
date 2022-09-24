#include "include/pygame.h"
#include "include/geometry.h"
#include "include/collisions.h"

#include <limits.h>
#include <float.h>
#include <stddef.h>
#include <math.h>

static PyObject *
_pg_polygon_vertices_aslist(pgPolygonBase *poly)
{
    return pg_PointList_FromArrayDouble(poly->vertices,
                                        (int)poly->verts_num * 2);
}

static PyObject *
_pg_polygon_vertices_astuple(pgPolygonBase *poly)
{
    return pg_PointTuple_FromArrayDouble(poly->vertices,
                                         (int)poly->verts_num * 2);
}

static int
pgPolygon_FromObject(PyObject *obj, pgPolygonBase *out)
{
    Py_ssize_t length;

    if (pgPolygon_Check(obj)) {
        pgPolygonBase *poly = &pgPolygon_AsPolygon(obj);

        out->verts_num = poly->verts_num;
        if (!out->vertices) {
            /* Only allocate new memory if the polygon vertices' memory is
             * not allocated, just copy the values otherwise*/
            out->vertices = PyMem_New(double, poly->verts_num * 2);
            if (!out->vertices) {
                return 0;
            }
        }

        memcpy(out->vertices, poly->vertices,
               poly->verts_num * 2 * sizeof(double));

        return 1;
    }

    if (PySequence_FAST_CHECK(obj)) {
        PyObject **f_arr = PySequence_Fast_ITEMS(obj);
        length = PySequence_Fast_GET_SIZE(obj);

        if (length >= 3) {
            Py_ssize_t i;
            out->verts_num = length;

            if (!out->vertices) {
                /* Only allocate new memory if the polygon vertices' memory is
                 * not allocated*/
                out->vertices = PyMem_New(double, length * 2);
                if (!out->vertices) {
                    return 0;
                }
            }

            for (i = 0; i < out->verts_num; i++) {
                if (!pg_TwoDoublesFromObj(f_arr[i], &(out->vertices[i * 2]),
                                          &(out->vertices[i * 2 + 1]))) {
                    return 0;
                }
            }
            return 1;
        }
        else if (length == 1) {
            if (!pgPolygon_FromObject(f_arr[0], out)) {
                return 0;
            }
            return 1;
        }

        /* Sequences of size 0 or 2 are not supported*/
        return 0;
    }
    else if (PySequence_Check(obj)) {
        /* Path for other sequences or Types that count as sequences*/
        PyObject *tmp = NULL;
        length = PySequence_Length(obj);

        if (length >= 3) {
            Py_ssize_t i;
            out->verts_num = length;
            out->vertices = PyMem_New(double, length * 2);
            if (!out->vertices) {
                return 0;
            }

            for (i = 0; i < length; i++) {
                tmp = PySequence_ITEM(obj, i);
                if (!pg_TwoDoublesFromObj(tmp, &(out->vertices[i]),
                                          &(out->vertices[i + 1]))) {
                    Py_DECREF(tmp);
                    return 0;
                }
                Py_DECREF(tmp);
                tmp = NULL;
            }

            return 1;
        }
        else if (length == 1) {
            tmp = PySequence_ITEM(obj, 0);
            if (PyUnicode_Check(obj) || !pgPolygon_FromObject(tmp, out)) {
                Py_DECREF(tmp);
                return 0;
            }
            Py_DECREF(tmp);
            return 1;
        }

        /* Sequences of size 0 or 2 are not supported*/
        return 0;
    }

    if (PyObject_HasAttrString(obj, "polygon")) {
        PyObject *polyattr;
        polyattr = PyObject_GetAttrString(obj, "polygon");
        if (polyattr == NULL) {
            PyErr_Clear();
            return 0;
        }
        if (PyCallable_Check(polyattr)) /*call if it's a method*/
        {
            PyObject *polyresult = PyObject_CallObject(polyattr, NULL);
            Py_DECREF(polyattr);
            if (polyresult == NULL) {
                PyErr_Clear();
                return 0;
            }
            polyattr = polyresult;
        }
        if (!pgPolygon_FromObject(polyattr, out)) {
            PyErr_Clear();
            Py_DECREF(polyattr);
            return 0;
        }
        Py_DECREF(polyattr);

        return 1;
    }

    return 0;
}

static int
pgPolygon_FromObjectFastcall(PyObject *const *args, Py_ssize_t nargs,
                             pgPolygonBase *out)
{
    if (nargs == 1) {
        return pgPolygon_FromObject(args[0], out);
    }
    else if (nargs >= 3) {
        Py_ssize_t i;
        out->verts_num = nargs;

        if (!out->vertices) {
            /* Only allocate new memory if the polygon vertices' memory is
             * not allocated*/
            out->vertices = PyMem_New(double, nargs * 2);
            if (!out->vertices) {
                return 0;
            }
        }

        for (i = 0; i < nargs; i++) {
            if (!pg_TwoDoublesFromObj(args[i], &(out->vertices[i * 2]),
                                      &(out->vertices[i * 2 + 1]))) {
                return 0;
            }
        }

        return 1;
    }

    return 0;
}

static int
pg_polygon_init(pgPolygonObject *self, PyObject *args, PyObject *kwds)
{
    if (!pgPolygon_FromObject(args, &(self->polygon))) {
        PyErr_SetString(PyExc_TypeError,
                        "Argument must be Polygon style object");
        return -1;
    }
    return 0;
}

static PyObject *
_pg_polygon_subtype_new2(PyTypeObject *type, double *vertices,
                         Py_ssize_t verts_num)
{
    pgPolygonObject *polygon_obj =
        (pgPolygonObject *)pgPolygon_Type.tp_new(type, NULL, NULL);

    if (verts_num < 3) {
        /*A polygon requires 3 or more vertices*/
        Py_DECREF(polygon_obj);
        return NULL;
    }

    if (polygon_obj) {
        polygon_obj->polygon.vertices = PyMem_New(double, verts_num * 2);
        if (!polygon_obj->polygon.vertices) {
            Py_DECREF(polygon_obj);
            return NULL;
        }

        memcpy(polygon_obj->polygon.vertices, vertices,
               verts_num * 2 * sizeof(double));

        polygon_obj->polygon.verts_num = verts_num;
    }

    return (PyObject *)polygon_obj;
}

static PyObject *
pg_polygon_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
    pgPolygonObject *self = (pgPolygonObject *)type->tp_alloc(type, 0);

    if (self) {
        self->polygon.vertices = NULL;
        self->polygon.verts_num = 0;
        self->weakreflist = NULL;
    }

    return (PyObject *)self;
}

static PyObject *
pgPolygon_New(pgPolygonBase *p)
{
    return _pg_polygon_subtype_new2(&pgPolygon_Type, p->vertices,
                                    p->verts_num);
}

static PyObject *
pgPolygon_New2(double *vertices, Py_ssize_t verts_num)
{
    return _pg_polygon_subtype_new2(&pgPolygon_Type, vertices, verts_num);
}

static void
pg_polygon_dealloc(pgPolygonObject *self)
{
    if (self->weakreflist != NULL) {
        PyObject_ClearWeakRefs((PyObject *)self);
    }

    PyMem_Free(self->polygon.vertices);

    Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *
pg_polygon_repr(pgPolygonObject *self)
{
    return PyUnicode_FromFormat("<Polygon(%S, %S)>",
                                PyLong_FromLong((int)self->polygon.verts_num),
                                _pg_polygon_vertices_aslist(&self->polygon));
}

static PyObject *
pg_polygon_str(pgPolygonObject *self)
{
    return pg_polygon_repr(self);
}

static PyObject *
pg_polygon_getsafepickle(pgPolygonObject *self, void *closure)
{
    Py_RETURN_TRUE;
}

static PyObject *
pg_polygon_copy(pgPolygonObject *self, PyObject *_null)
{
    return pgPolygon_New(&self->polygon);
}

static struct PyMethodDef pg_polygon_methods[] = {
    {"__copy__", (PyCFunction)pg_polygon_copy, METH_NOARGS, NULL},
    {"copy", (PyCFunction)pg_polygon_copy, METH_NOARGS, NULL},
    {NULL, NULL, 0, NULL}};

static PyObject *
pg_polygon_get_verts_num(pgPolygonObject *self, void *closure)
{
    return PyLong_FromLong((int)self->polygon.verts_num);
}

static PyObject *
pg_polygon_get_vertices(pgPolygonObject *self, void *closure)
{
    return _pg_polygon_vertices_aslist(&self->polygon);
}

static int
pg_polygon_ass_vertex(pgPolygonObject *self, Py_ssize_t i, PyObject *v)
{
    pgPolygonBase *poly = &self->polygon;

    if (i < 0) {
        i += poly->verts_num;
    }

    if (i < 0 || i > poly->verts_num) {
        PyErr_SetString(PyExc_IndexError, "Invalid vertex Index");
        return -1;
    }

    double Vx, Vy;
    if (!pg_TwoDoublesFromObj(v, &Vx, &Vy)) {
        PyErr_SetString(PyExc_TypeError, "Must assign numeric values");
        return -1;
    }
    poly->vertices[i * 2] = Vx;
    poly->vertices[i * 2 + 1] = Vy;
    return 0;
}

static Py_ssize_t
pg_polygon_seq_length(pgPolygonObject *self)
{
    return pgPolygon_GETVERTSNUM(self);
}

static int
pg_polygon_ass_subscript(pgPolygonObject *self, PyObject *op, PyObject *value)
{
    if (PyIndex_Check(op)) {
        PyObject *index;
        Py_ssize_t i;

        index = PyNumber_Index(op);
        if (index == NULL) {
            return -1;
        }
        i = PyNumber_AsSsize_t(index, NULL);
        Py_DECREF(index);
        return pg_polygon_ass_vertex(self, i, value);
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Expected a number or sequence");
        return -1;
    }
}

static PyObject *
pg_polygon_subscript(pgPolygonObject *self, PyObject *op)
{
    PyObject *index = PyNumber_Index(op);
    Py_ssize_t i;

    if (index == NULL) {
        return NULL;
    }
    i = PyNumber_AsSsize_t(index, NULL);
    Py_DECREF(index);

    pgPolygonBase *poly = &self->polygon;

    if (i < 0) {
        i += poly->verts_num;
    }

    if (i < 0 || i > poly->verts_num) {
        return RAISE(PyExc_IndexError, "Invalid vertex Index");
    }

    return pg_TupleFromDoublePair(poly->vertices[i * 2], poly->vertices[i * 2 + 1]);
}

static int
pg_polygon_contains_seq(pgPolygonObject *self, PyObject *arg)
{
    double x, y;
    if (PySequence_FAST_CHECK(arg)) {
        if (!pg_TwoDoublesFromObj(arg, &x, &y)) {
            PyErr_SetString(PyExc_TypeError, "Expected a sequence of 2 numbers");
            return -1;
        }
        else if (PySequence_Check(arg)) {
            if (PySequence_Size(arg) < 2) {
                PyErr_SetString(PyExc_TypeError, "Expected a sequence of 2 numbers");
                return -1;
            }
            PyObject *tmp = PySequence_GetItem(arg, 0);
            if (!tmp) {
                return -1;
            }
            
            if (!pg_DoubleFromObj(tmp, &x)) {
                PyErr_SetString(PyExc_TypeError, "Expected a sequence of 2 numbers");
                return -1;
            }
            Py_DECREF(tmp);

            tmp = PySequence_GetItem(arg, 1);
            if (!tmp) {
                return NULL;
            }
            
            if (!pg_DoubleFromObj(tmp, &y)) {
                PyErr_SetString(PyExc_TypeError, "Expected a sequence of 2 numbers");
                return -1;
            }
            Py_DECREF(tmp);
        }
        else {
            PyErr_SetString(PyExc_TypeError, "Expected a sequence");
            return -1;
        }

        pgPolygonBase *poly = &self->polygon;
        Py_ssize_t i;
        for (i = 0; i < poly->verts_num; i+=2) {
            if (poly->vertices[i] == x && poly->vertices[i+1] == y) {
                return 1;
            }
        }
    }
    else {
        PyErr_SetString(PyExc_TypeError, "Expected a sequence");
        return -1;
    }
    return 0;
}

static PyMappingMethods pg_polygon_as_mapping = {
    .mp_subscript = (binaryfunc)pg_polygon_subscript,
    .mp_ass_subscript = (objobjargproc)pg_polygon_ass_subscript,
    .mp_length = (lenfunc)pg_polygon_seq_length};

static PySequenceMethods pg_polygon_as_sequence = {
    .sq_length = (lenfunc)pg_polygon_seq_length,
    .sq_item = (ssizeargfunc)pg_polygon_subscript,
    .sq_ass_item = (ssizeobjargproc)pg_polygon_ass_subscript,
    .sq_contains = (objobjproc)pg_polygon_contains_seq,
};

static PyGetSetDef pg_polygon_getsets[] = {
    {"verts_num", (getter)pg_polygon_get_verts_num, NULL,
     "Number of vertices of the polygon", NULL},
    {"vertices", (getter)pg_polygon_get_vertices, NULL,
     "Vertices of the polygon", NULL},
    {"__safe_for_unpickling__", (getter)pg_polygon_getsafepickle, NULL, NULL,
     NULL},
    {NULL, 0, NULL, NULL, NULL} /* Sentinel */
};

static PyTypeObject pgPolygon_Type = {
    PyVarObject_HEAD_INIT(NULL, 0).tp_name = "pygame.Polygon",
    .tp_basicsize = sizeof(pgPolygonObject),
    .tp_dealloc = (destructor)pg_polygon_dealloc,
    .tp_repr = (reprfunc)pg_polygon_repr,
    .tp_str = (reprfunc)pg_polygon_str,
    .tp_as_mapping = &pg_polygon_as_mapping,
    .tp_as_sequence = &pg_polygon_as_sequence,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_doc = NULL,
    .tp_weaklistoffset = offsetof(pgPolygonObject, weakreflist),
    .tp_methods = pg_polygon_methods,
    .tp_getset = pg_polygon_getsets,
    .tp_init = (initproc)pg_polygon_init,
    .tp_new = pg_polygon_new,
};