==============
PerStateOutput
==============

This plugin splits log and debug output across one directory for
each symbolic state.

Setting up PerStateOutput Plugin
================================

To enable the ``PerStateOutput`` plugin in the S2E configuration file,
add the following lines to your ``config.lua`` file:

.. code-block:: lua

   plugins = {
     ...
     "PerStateOutput"
   }
