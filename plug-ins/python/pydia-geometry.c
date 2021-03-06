/* Python plug-in for dia
 * Copyright (C) 1999  James Henstridge
 * Copyright (C) 2000  Hans Breuer
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>

#include "pydia-object.h"
#include "pydia-geometry.h"


/* Implements wrappers for Point, Rectangle, IntRectangle, BezPoint */

/*
 * New
 */
PyObject* PyDiaPoint_New (Point* pt)
{
  PyDiaPoint *self;
  
  self = PyObject_NEW(PyDiaPoint, &PyDiaPoint_Type);
  if (!self) return NULL;
  
  self->pt = *pt;

  return (PyObject *)self;
}

PyObject* 
PyDiaPointTuple_New (Point* pts, int num)
{
  PyObject* ret;
  int i;

  ret = PyTuple_New (num);
  if (ret) {
    for (i = 0; i < num; i++)
      PyTuple_SetItem(ret, i, PyDiaPoint_New(&(pts[i])));
  }

  return ret;
}

/* one of the parameters needs to be NULL, the other is created */
PyObject* 
PyDiaRectangle_New (Rectangle* r, IntRectangle* ri)
{
  PyDiaRectangle *self;

  self = PyObject_NEW(PyDiaRectangle, &PyDiaRectangle_Type);
  if (!self) return NULL;

  self->is_int = (ri != NULL);
  if (self->is_int)
    self->r.ri = *ri;
  else
    self->r.rf = *r;

  return (PyObject *)self;
}

PyObject* PyDiaRectangle_New_FromPoints (Point* ul, Point* lr)
{
  PyDiaRectangle *self;

  self = PyObject_NEW(PyDiaRectangle, &PyDiaRectangle_Type);
  if (!self) return NULL;

  self->is_int = FALSE;
  self->r.rf.left = ul->x;
  self->r.rf.top = ul->y;
  self->r.rf.right = lr->x;
  self->r.rf.bottom = lr->y;

  return (PyObject *)self;
}


PyObject* PyDiaBezPoint_New (BezPoint* bpn)
{
  PyDiaBezPoint *self;
  
  self = PyObject_NEW(PyDiaBezPoint, &PyDiaBezPoint_Type);
  if (!self) return NULL;
  
  self->bpn = *bpn;

  return (PyObject *)self;
}

PyObject* 
PyDiaBezPointTuple_New (BezPoint* pts, int num)
{
  PyObject* ret;
  int i;

  ret = PyTuple_New (num);
  if (ret) {
    for (i = 0; i < num; i++)
      PyTuple_SetItem(ret, i, PyDiaBezPoint_New(&(pts[i])));
  }

  return ret;
}

PyObject* PyDiaArrow_New (Arrow* arrow)
{
  PyDiaArrow *self;
  
  self = PyObject_NEW(PyDiaArrow, &PyDiaArrow_Type);
  if (!self) return NULL;
  
  self->arrow = *arrow;

  return (PyObject *)self;
}

/*
 * Dealloc
 */
static void
PyDiaGeometry_Dealloc(void *self)
{
     PyObject_DEL(self);
}

/*
 * Compare ?
 */
static int
PyDiaPoint_Compare(PyDiaPoint *self,
			     PyDiaPoint *other)
{
#if 1
  return memcmp (&self->pt, &other->pt, sizeof(Point));
#else /* ? */
  if (self->pt.x == other->pt.x && self->pt.x == other->pt.x) return 0;
#define SQR(pt) (pt.x*pt.y)
  if (SQR(self->pt) > SQR(other->pt)) return -1;
#undef  SQR 
  return 1;
#endif
}

static int
PyDiaRectangle_Compare(PyDiaRectangle *self,
			     PyDiaRectangle *other)
{
  /* this is not correct */
  return memcmp (&self->r, &other->r, sizeof(Rectangle));
}

static int
PyDiaBezPoint_Compare(PyDiaBezPoint *self,
			     PyDiaBezPoint *other)
{
  return memcmp (&self->bpn, &other->bpn, sizeof(BezPoint));
}

static int
PyDiaArrow_Compare(PyDiaArrow *self,
			 PyDiaArrow *other)
{
  return memcmp (&self->arrow, &other->arrow, sizeof(Arrow));
}

/*
 * Hash
 */
static long
PyDiaGeometry_Hash(PyObject *self)
{
    return (long)self;
}

/*
 * GetAttr
 */
static PyObject *
PyDiaPoint_GetAttr(PyDiaPoint *self, gchar *attr)
{
  if (!strcmp(attr, "__members__"))
    return Py_BuildValue("[ss]", "x", "y");
  else if (!strcmp(attr, "x"))
    return PyFloat_FromDouble(self->pt.x);
  else if (!strcmp(attr, "y"))
    return PyFloat_FromDouble(self->pt.y);

  PyErr_SetString(PyExc_AttributeError, attr);
  return NULL;
}

static PyObject *
PyDiaRectangle_GetAttr(PyDiaRectangle *self, gchar *attr)
{
#define I_OR_F(v) \
  (self->is_int ? \
   PyInt_FromLong(self->r.ri. v) : PyFloat_FromDouble(self->r.rf. v))

  if (!strcmp(attr, "__members__"))
    return Py_BuildValue("[ssss]", "top", "left", "right", "bottom" );
  else if (!strcmp(attr, "top"))
    return I_OR_F(top);
  else if (!strcmp(attr, "left"))
    return I_OR_F(left);
  else if (!strcmp(attr, "right"))
    return I_OR_F(right);
  else if (!strcmp(attr, "bottom"))
    return I_OR_F(bottom);

  PyErr_SetString(PyExc_AttributeError, attr);
  return NULL;

#undef I_O_F
}

static PyObject *
PyDiaBezPoint_GetAttr(PyDiaBezPoint *self, gchar *attr)
{
  if (!strcmp(attr, "__members__"))
    return Py_BuildValue("[ssss]", "type", "p1", "p2", "p3");
  else if (!strcmp(attr, "type"))
    return PyInt_FromLong(self->bpn.type);
  else if (!strcmp(attr, "p1"))
    return PyDiaPoint_New(&(self->bpn.p1));
  else if (!strcmp(attr, "p2"))
    return PyDiaPoint_New(&(self->bpn.p2));
  else if (!strcmp(attr, "p3"))
    return PyDiaPoint_New(&(self->bpn.p3));

  PyErr_SetString(PyExc_AttributeError, attr);
  return NULL;
}

static PyObject *
PyDiaArrow_GetAttr(PyDiaArrow *self, gchar *attr)
{
  if (!strcmp(attr, "__members__"))
    return Py_BuildValue("[sss]", "type", "width", "length");
  else if (!strcmp(attr, "type"))
    return PyInt_FromLong(self->arrow.type);
  else if (!strcmp(attr, "width"))
    return PyFloat_FromDouble(self->arrow.width);
  else if (!strcmp(attr, "length"))
    return PyFloat_FromDouble(self->arrow.length);

  PyErr_SetString(PyExc_AttributeError, attr);
  return NULL;
}

/*
 * SetAttr
 */

/* XXX */

/*
 * Repr / _Str
 */
static PyObject *
PyDiaPoint_Str(PyDiaPoint *self)
{
    PyObject* py_s;
#ifndef _DEBUG /* gives crashes with nan */
    gchar* s = g_strdup_printf ("(%f,%f)",
                                (float)(self->pt.x), (float)(self->pt.y));
#else
    gchar* s = g_strdup_printf ("(%e,%e)",
                                (float)(self->pt.x), (float)(self->pt.y));
#endif
    py_s = PyString_FromString(s);
    g_free(s);
    return py_s;
}

static PyObject *
PyDiaRectangle_Str(PyDiaRectangle *self)
{
    PyObject* py_s;
    gchar* s;
    if (self->is_int)
      s = g_strdup_printf ("((%d,%d),(%d,%d))",
                           (self->r.ri.left), (self->r.ri.top),
                           (self->r.ri.right), (self->r.ri.bottom));
    else
#ifndef _DEBUG /* gives crashes with nan */
      s = g_strdup_printf ("((%f,%f),(%f,%f))",
                           (float)(self->r.rf.left), (float)(self->r.rf.top),
                           (float)(self->r.rf.right), (float)(self->r.rf.bottom));
#else
      s = g_strdup_printf ("((%e,%e),(%e,%e))",
                           (float)(self->r.rf.left), (float)(self->r.rf.top),
                           (float)(self->r.rf.right), (float)(self->r.rf.bottom));
#endif
    py_s = PyString_FromString(s);
    g_free(s);
    return py_s;
}

static PyObject *
PyDiaBezPoint_Str(PyDiaBezPoint *self)
{
    PyObject* py_s;
#if 0 /* FIXME: this is causing bad crashes with unintialized points. 
       * Probably a glib and a Dia problem ... */
    gchar* s = g_strdup_printf ("((%f,%f),(%f,%f),(%f,%f),%s)",
                                (float)(self->bpn.p1.x), (float)(self->bpn.p1.y),
                                (float)(self->bpn.p2.x), (float)(self->bpn.p2.y),
                                (float)(self->bpn.p3.x), (float)(self->bpn.p3.y),
                                (self->bpn.type == BEZ_MOVE_TO ? "MOVE_TO" :
                                  (self->bpn.type == BEZ_LINE_TO ? "LINE_TO" : "CURVE_TO")));
#else
    gchar* s = g_strdup_printf ("%s",
                                (self->bpn.type == BEZ_MOVE_TO ? "MOVE_TO" :
                                  (self->bpn.type == BEZ_LINE_TO ? "LINE_TO" : "CURVE_TO")));
#endif
    py_s = PyString_FromString(s);
    g_free(s);
    return py_s;
}


static PyObject *
PyDiaArrow_Str(PyDiaArrow *self)
{
    PyObject* py_s;
    gchar* s = g_strdup_printf ("(%f,%f, %d)",
                                (float)(self->arrow.width), 
                                (float)(self->arrow.length),
                                (int)(self->arrow.type));
    py_s = PyString_FromString(s);
    g_free(s);
    return py_s;
}

/* 
 * sequence interface (query only) 
 */
/* Point */
static int
point_length(PyDiaRectangle *self)
{
  return 2;
}
static PyObject *
point_item(PyDiaPoint* self, int i)
{
  switch (i) {
  case 0 : return PyDiaPoint_GetAttr(self, "x");
  case 1 : return PyDiaPoint_GetAttr(self, "y");
  default :
    PyErr_SetString(PyExc_IndexError, "PyDiaPoint index out of range");
    return NULL;
  }
}
static PyObject *
point_slice(PyDiaPoint* self, int i, int j)
{
  PyObject *ret;

  /* j maybe negative */
  if (j <= 0)
    j = 1 + j;
  /* j may be rather huge [:] ^= 0:0x7FFFFFFF */
  if (j > 1)
    j = 1;
  ret = PyTuple_New(j - i + 1);
  if (ret) {
    int k;
    for (k = i; k <= j && k < 2; k++)
      PyTuple_SetItem(ret, k - i, point_item(self, k)); 
  }
  return ret;
}

static PySequenceMethods point_as_sequence = {
	(inquiry)point_length, /*sq_length*/
	(binaryfunc)0, /*sq_concat*/
	(intargfunc)0, /*sq_repeat*/
	(intargfunc)point_item, /*sq_item*/
	(intintargfunc)point_slice, /*sq_slice*/
	0,		/*sq_ass_item*/
	0,		/*sq_ass_slice*/
	(objobjproc)0 /*sq_contains*/
};

/* Rect */
static int
rect_length(PyDiaRectangle *self)
{
  return 4;
}
static PyObject *
rect_item(PyDiaRectangle* self, int i)
{
  switch (i) {
  case 0 : return PyDiaRectangle_GetAttr(self, "left");
  case 1 : return PyDiaRectangle_GetAttr(self, "top");
  case 2 : return PyDiaRectangle_GetAttr(self, "right");
  case 3 : return PyDiaRectangle_GetAttr(self, "bottom");
  default :
    PyErr_SetString(PyExc_IndexError, "PyDiaRectangle index out of range");
    return NULL;
  }
}
static PyObject *
rect_slice(PyDiaRectangle* self, int i, int j)
{
  PyObject *ret;

  /* j maybe negative */
  if (j <= 0)
    j = 3 + j;
  /* j may be rather huge [:] ^= 0:0x7FFFFFFF */
  if (j > 3)
    j = 3;
  ret = PyTuple_New(j - i + 1);
  if (ret) {
    int k;
    for (k = i; k <= j && k < 4; k++)
      PyTuple_SetItem(ret, k - i, rect_item(self, k)); 
  }
  return ret;
}

static PySequenceMethods rect_as_sequence = {
	(inquiry)rect_length, /*sq_length*/
	(binaryfunc)0, /*sq_concat*/
	(intargfunc)0, /*sq_repeat*/
	(intargfunc)rect_item, /*sq_item*/
	(intintargfunc)rect_slice, /*sq_slice*/
	0,		/*sq_ass_item*/
	0,		/*sq_ass_slice*/
	(objobjproc)0 /*sq_contains*/
};

/*
 * Python objetcs
 */
PyTypeObject PyDiaPoint_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "dia.Point",
    sizeof(PyDiaPoint),
    0,
    (destructor)PyDiaGeometry_Dealloc,
    (printfunc)0,
    (getattrfunc)PyDiaPoint_GetAttr,
    (setattrfunc)0,
    (cmpfunc)PyDiaPoint_Compare,
    (reprfunc)0,
    0, /* as_number */
    &point_as_sequence,
    0,
    (hashfunc)PyDiaGeometry_Hash,
    (ternaryfunc)0,
    (reprfunc)PyDiaPoint_Str,
    (getattrofunc)0,
    (setattrofunc)0,
    (PyBufferProcs *)0,
    0L, /* Flags */
    "The dia.Point does not only provide access trough it's members but also via a sequence interface."
};

PyTypeObject PyDiaRectangle_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "dia.Rectangle",
    sizeof(PyDiaRectangle),
    0,
    (destructor)PyDiaGeometry_Dealloc,
    (printfunc)0,
    (getattrfunc)PyDiaRectangle_GetAttr,
    (setattrfunc)0,
    (cmpfunc)PyDiaRectangle_Compare,
    (reprfunc)0,
    0, /* as_number */
    &rect_as_sequence,
    0, /* as_mapping */
    (hashfunc)PyDiaGeometry_Hash,
    (ternaryfunc)0,
    (reprfunc)PyDiaRectangle_Str,
    (getattrofunc)0,
    (setattrofunc)0,
    (PyBufferProcs *)0,
    0L, /* Flags */
    "The dia.Rectangle does not only provide access trough it's members but also via a sequence interface."
};

PyTypeObject PyDiaBezPoint_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "dia.BezPoint",
    sizeof(PyDiaBezPoint),
    0,
    (destructor)PyDiaGeometry_Dealloc,
    (printfunc)0,
    (getattrfunc)PyDiaBezPoint_GetAttr,
    (setattrfunc)0,
    (cmpfunc)PyDiaBezPoint_Compare,
    (reprfunc)0,
    0,
    0,
    0,
    (hashfunc)PyDiaGeometry_Hash,
    (ternaryfunc)0,
    (reprfunc)PyDiaBezPoint_Str,
    (getattrofunc)0,
    (setattrofunc)0,
    (PyBufferProcs *)0,
    0L, /* Flags */
    "A dia.Point, a bezier type and two control points (dia.Point) make a bezier point."
};

PyTypeObject PyDiaArrow_Type = {
    PyObject_HEAD_INIT(&PyType_Type)
    0,
    "dia.Arrow",
    sizeof(PyDiaArrow),
    0,
    (destructor)PyDiaGeometry_Dealloc,
    (printfunc)0,
    (getattrfunc)PyDiaArrow_GetAttr,
    (setattrfunc)0,
    (cmpfunc)PyDiaArrow_Compare,
    (reprfunc)0,
    0,
    0,
    0,
    (hashfunc)PyDiaGeometry_Hash,
    (ternaryfunc)0,
    (reprfunc)PyDiaArrow_Str,
    (getattrofunc)0,
    (setattrofunc)0,
    (PyBufferProcs *)0,
    0L, /* Flags */
    "Dia's line objects usually ends with an dia.Arrow"
};
