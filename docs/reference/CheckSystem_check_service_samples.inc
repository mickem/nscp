Default check::

	check_service
	OK all services are ok.

**Excluding services using exclude**::

	check_service "exclude=clr_optimization_v4.0.30319_32"  "exclude=clr_optimization_v4.0.30319_64"
	WARNING: gupdate=stopped (auto), Net Driver HPZ12=stopped (auto), NSClientpp=stopped (auto), nscp=stopped (auto), Pml Driver HPZ12=stopped (auto), SkypeUpdate=stopped (auto), sppsvc=stopped (auto)

**Show all service by changing the syntax**::

	check_service "top-syntax=${list}" "detail-syntax=${name}:${state}"
	AdobeActiveFileMonitor10.0:running, AdobeARMservice:running, AdobeFlashPlayerUpdateSvc:stopped, ..., WwanSvc:stopped

**Excluding services using the filter**::

	check_service "filter=start_type = 'auto' and name not in ('Bonjour Service', 'Net Driver HPZ12')"
	AdobeActiveFileMonitor10.0: running, AdobeARMservice: running, AMD External Events Utility: running,  ... wuauserv: running

Default check **via NRPE**::

	check_nrpe --host 192.168.56.103 --command check_service
	WARNING: DPS=stopped (auto), MSDTC=stopped (auto), sppsvc=stopped (auto), UALSVC=stopped (auto)
