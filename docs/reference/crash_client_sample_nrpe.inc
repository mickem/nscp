How to use the crashclient command via NRPE
-------------------------------------------

Configuration to setup the module:

.. code-block:: ini

   [/modules]
   NRPEServer = enabled
   CauseCrashes = enabled

   [/settings/NRPE/server]
   allowed hosts = 127.0.0.1

Then execute the following command:

.. code-block:: bat

   nscp nrpe --host 127.0.0.1 --command crashclient
