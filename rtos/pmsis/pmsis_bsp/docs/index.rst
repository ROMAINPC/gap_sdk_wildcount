PMSIS BSP
=========

Introduction
------------

The PMSIS BSP is a set of high level-drivers written on top of the PMSIS API,
which makes them available on any operating system which implements the PMSIS API.

Conventions
-----------

All functions prefixed by :code:`pi_` can only be called from fabric-controller
side while the ones prefixed by :code:`pi_cl_` can only be called from cluster
side. Any exception to these rules is documented where it applies.

All functions on fabric-controller side are by default synchronous and are
blocking the caller until the operation is done. All the functions suffixed by
:code:`_async` are asynchronous and are not blocking the caller. The
termination of such operations is managed with a :code:`pi_task_t` object, see
PMSIS API documentation for more information.

Functions on cluster-side are by default synchronous but can also be
asynchronous if the documentation of the function mentions it.

Chip-specific information
-------------------------

GAP8
####

All functions transferring data between an external device and a chip memory
must use the L2 memory for the chip memory.

Drivers
-------

.. toctree::
   :maxdepth: 2

   adc.rst
   ble.rst
   camera.rst
   display.rst
   flash.rst
   fs.rst
   gpio.rst
   ram.rst
