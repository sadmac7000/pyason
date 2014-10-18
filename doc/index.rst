.. pyason documentation master file, created by
   sphinx-quickstart on Sun Oct 12 18:48:00 2014.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

.. currentmodule:: ason

Welcome to pyason's documentation!
==================================

``pyason`` is a library for manipulating ASON [#f1]_ values in python.

``pyason`` provides the :py:mod:`ason` module, which, in turn, provides a
:py:func:`parse` function for parsing ASON values and the :py:class:`ason`
constructor for converting Python values to ASON.

ASON values are manipulated as objects of type :py:class:`ason`. Standard
ASON operations are all mapped to Python operators.

        >>> from ason import ason
        >>> a = ason(6)
        >>> b = ason("bob")
        >>> a
        ason(6)
        >>> b
        ason("bob")
        >>> a | b
        ason(6 ∪ "bob")
        >>> ~a
        ason(!6)

Also supported is the ``&`` operator for ASON intersection. Comparison maps
representation to equality, with ``a <= b`` indicating ``a`` is represented in
``b``. This is designed to mirror Python's :py:class:`set` class. The join
operator can be accessed with the :py:meth:`ason.join` method. All operators
will attempt to promote the right-hand operand to an :py:class:`ason.ason`
object, so ``ason(6) | 7`` should yield ``ason(6 ∪ 7)``.

The :py:class:`ason` class is also iterable and castable to many types, which
can be used to extract ASON values as python values. Example:

        >>> list(ason.parse("6"))
        [ason(6)]
        >>> list(ason.parse("6 | 7"))
        [ason(6), ason(7)]
        >>> list(ason.parse("[6,7]"))
        [ason(6), ason(7)]
        >>> dict(ason.parse('{"foo":6, "bar":7, *}'))
        {'foo': ason(6), 'bar': ason(7)}
        >>> str(ason.ason("hi"))
        'hi'
        >>> str(ason.ason(6))
          TypeError: ASON type is not a string
        >>> int(ason.ason(7))
        7L


Functions
=========
.. autofunction:: parse(string, \**args)

.. autofunction:: uobject(value, \**args)

The ason class
==============
.. autoclass:: ason
   :members: is_complement, is_list, is_numeric, is_object, is_string, is_union, serialize

   .. automethod:: join(other)

Constants
=========
.. py:data:: U

   The ASON Universe value ("``U``") as an :py:class:`ason` object

.. py:data:: WILD

   The ASON Wild value ("``*``")as an :py:class:`ason` object

.. py:data:: EMPTY

   The ASON Empty value (∅) as an :py:class:`ason` object

.. [#f1] http://www.americanteeth.org/libason/ason_spec.pdf
