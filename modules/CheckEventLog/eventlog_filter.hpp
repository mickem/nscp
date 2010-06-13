#pragma once

struct eventlog_filter {
	filters::filter_all_strings eventSource;
	filters::filter_all_numeric<unsigned int, filters::handlers::eventtype_handler> eventType;
	filters::filter_all_numeric<unsigned int, filters::handlers::eventseverity_handler> eventSeverity;
	filters::filter_all_strings message;
	filters::filter_all_times timeWritten;
	filters::filter_all_times timeGenerated;
	filters::filter_all_numeric<DWORD, filters::handlers::eventtype_handler> eventID;
	std::wstring value_;

	inline bool hasFilter() {
		return eventSource.hasFilter() || eventType.hasFilter() || eventID.hasFilter() || eventSeverity.hasFilter() || message.hasFilter() || 
			timeWritten.hasFilter() || timeGenerated.hasFilter();
	}

#define NSCP_EL_DEBUG(key) if (key.hasFilter()) strEx::append_list(str, std::wstring(_T( # key )) + _T(" ") + key.to_string(), _T(","));
	std::wstring to_string() const {
		std::wstring str;
		NSCP_EL_DEBUG(eventSource);
		NSCP_EL_DEBUG(eventType);
		NSCP_EL_DEBUG(eventSeverity);
		NSCP_EL_DEBUG(eventID);
		NSCP_EL_DEBUG(message);
		NSCP_EL_DEBUG(timeWritten);
		NSCP_EL_DEBUG(timeGenerated);
		return str;
	}
	bool matchFilter(const EventLogRecord &value) const {
		bool ret = false;
		if ((eventSource.hasFilter())&&(eventSource.matchFilter(value.eventSource())))
			ret = true;
		else if (eventSource.hasFilter())
			return false;
		else if ((eventType.hasFilter())&&(eventType.matchFilter(value.eventType())))
			ret = true;
		else if (eventType.hasFilter())
			return false;
		else if ((eventSeverity.hasFilter())&&(eventSeverity.matchFilter(value.severity())))
			ret = true;
		else if (eventSeverity.hasFilter())
			return false;
		else if ((eventID.hasFilter())&&(eventID.matchFilter(value.eventID()))) 
			ret = true;
		else if (eventID.hasFilter())
			return false;
		else if ((message.hasFilter())&&(message.matchFilter(value.enumStrings())))
			ret = true;
		else if (message.hasFilter())
			return false;
		else if ((timeWritten.hasFilter())&&(timeWritten.matchFilter(value.timeWritten())))
			ret = true;
		else if (timeWritten.hasFilter())
			return false;
		else if ((timeGenerated.hasFilter())&&(timeGenerated.matchFilter(value.timeGenerated())))
			ret = true;
		else if (timeGenerated.hasFilter())
			return false;
		return ret;
	}
};