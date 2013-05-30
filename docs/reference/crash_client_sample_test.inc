How to use the crashclient command
----------------------------------

Configuration to setup the module:

.. code-block:: ini

   [/modules]
   CauseCrashes = enabled

Then execute the following command:

.. code-block:: text

   nscp test
   ...
   crashclient

This will cause NSClient++ to crash, you can also crash the client remotely using the NRPEServer like so: