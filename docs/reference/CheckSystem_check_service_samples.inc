Default check::

	check_service
	OK all services are ok.

**Excluding services using the filter**::

	check_service "filter=start_type = 'auto' and name not in ('Bonjour Service', 'Net Driver HPZ12')" "top-syntax=${list}" "detail-syntax=${name}: ${state}"
	AdobeActiveFileMonitor10.0: running, AdobeARMservice: running, AMD External Events Utility: running,  ... wuauserv: running

Default check **via NRPE**::

	check_nrpe --host 192.168.56.103 --command check_service
	DPS=stopped (auto), MSDTC=stopped (auto), sppsvc=stopped (auto), UALSVC=stopped (auto)