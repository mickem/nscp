#pragma once

#include <error.hpp>

#include "modern_eventlog.hpp"
#include <buffer.hpp>

namespace eventlog {
	namespace api {
		tEvtOpenPublisherEnum pEvtOpenPublisherEnum = NULL;
		tEvtClose pEvtClose = NULL;
		tEvtNextPublisherId ptEvtNextPublisherId = NULL;
		tEvtOpenChannelEnum pEvtOpenChannelEnum = NULL;
		tEvtNextChannelPath pEvtNextChannelPath = NULL;
		tEvtGetPublisherMetadataProperty pEvtGetPublisherMetadataProperty = NULL;
		tEvtGetObjectArrayProperty pEvtGetObjectArrayProperty = NULL;
		tEvtGetObjectArraySize pEvtGetObjectArraySize = NULL;
		tEvtQuery pEvtQuery = NULL;
		tEvtNext pEvtNext = NULL;
		tEvtSeek pEvtSeek = NULL;
		tEvtCreateRenderContext pEvtCreateRenderContext = NULL;
		tEvtRender pEvtRender = NULL;
		tEvtOpenPublisherMetadata pEvtOpenPublisherMetadata = NULL;
		tEvtFormatMessage pEvtFormatMessage = NULL;

		void load_procs() {
			HMODULE hModule = LoadLibrary(_TEXT("Wevtapi.dll"));
			pEvtFormatMessage = reinterpret_cast<api::tEvtFormatMessage>(GetProcAddress(hModule, "EvtFormatMessage"));
			pEvtOpenPublisherMetadata = reinterpret_cast<api::tEvtOpenPublisherMetadata>(GetProcAddress(hModule, "EvtOpenPublisherMetadata"));
			pEvtCreateRenderContext = reinterpret_cast<api::tEvtCreateRenderContext>(GetProcAddress(hModule, "EvtCreateRenderContext"));
			pEvtRender = reinterpret_cast<api::tEvtRender>(GetProcAddress(hModule, "EvtRender"));
			pEvtSeek = reinterpret_cast<api::tEvtSeek>(GetProcAddress(hModule, "EvtSeek"));
			pEvtNext = reinterpret_cast<api::tEvtNext>(GetProcAddress(hModule, "EvtNext"));
			pEvtOpenPublisherEnum = reinterpret_cast<api::tEvtOpenPublisherEnum>(GetProcAddress(hModule, "EvtOpenPublisherEnum"));
			pEvtQuery = reinterpret_cast<api::tEvtQuery>(GetProcAddress(hModule, "EvtQuery"));
			pEvtClose = reinterpret_cast<api::tEvtClose>(GetProcAddress(hModule, "EvtClose"));
			pEvtNextChannelPath = reinterpret_cast<api::tEvtNextChannelPath>(GetProcAddress(hModule, "EvtNextChannelPath"));
			pEvtOpenChannelEnum = reinterpret_cast<api::tEvtOpenChannelEnum>(GetProcAddress(hModule, "EvtOpenChannelEnum"));
			ptEvtNextPublisherId = reinterpret_cast<api::tEvtNextPublisherId>(GetProcAddress(hModule, "EvtNextPublisherId"));
			pEvtGetPublisherMetadataProperty = reinterpret_cast<api::tEvtGetPublisherMetadataProperty>(GetProcAddress(hModule, "EvtGetPublisherMetadataProperty"));
			pEvtGetObjectArrayProperty = reinterpret_cast<api::tEvtGetObjectArrayProperty>(GetProcAddress(hModule, "EvtGetObjectArrayProperty"));
			pEvtGetObjectArraySize = reinterpret_cast<api::tEvtGetObjectArraySize>(GetProcAddress(hModule, "EvtGetObjectArraySize"));
		}
		bool supports_modern() {
			return pEvtQuery != NULL;
		}
	}
	BOOL EvtFormatMessage(api::EVT_HANDLE PublisherMetadata, api::EVT_HANDLE Event, DWORD MessageId, DWORD ValueCount, api::PEVT_VARIANT Values, DWORD Flags, DWORD BufferSize, LPWSTR Buffer, PDWORD BufferUsed) {
		if (!api::pEvtFormatMessage)
			throw nscp_exception("Failed to load: EvtFormatMessage");
		return api::pEvtFormatMessage(PublisherMetadata, Event, MessageId, ValueCount, Values, Flags, BufferSize, Buffer, BufferUsed);
	}
	int EvtFormatMessage(api::EVT_HANDLE PublisherMetadata, api::EVT_HANDLE Event, DWORD MessageId, DWORD ValueCount, api::PEVT_VARIANT Values, DWORD Flags, std::string &str) {
		hlp::buffer<wchar_t, LPWSTR> message_buffer(4096);
		DWORD dwBufferSize;
		if (!eventlog::EvtFormatMessage(PublisherMetadata, Event, MessageId, ValueCount, Values, Flags, static_cast<DWORD>(message_buffer.size()), message_buffer.get(), &dwBufferSize)) {
			DWORD status = GetLastError();
			if (status == ERROR_INSUFFICIENT_BUFFER) {
				message_buffer.resize(dwBufferSize + 10);
				if (!eventlog::EvtFormatMessage(PublisherMetadata, Event, MessageId, ValueCount, Values, Flags, static_cast<DWORD>(message_buffer.size()), message_buffer.get(), &dwBufferSize)) {
					return GetLastError();
				}
			}
			return status;
		}
		str = utf8::cvt<std::string>((wchar_t*)message_buffer);
		return ERROR_SUCCESS;
	}




	api::EVT_HANDLE EvtOpenPublisherMetadata(api::EVT_HANDLE Session, LPCWSTR PublisherId, LPCWSTR LogFilePath, LCID Locale, DWORD Flags) {
		if (!api::pEvtOpenPublisherMetadata)
			throw nscp_exception("Failed to load: EvtOpenPublisherMetadata");
		return api::pEvtOpenPublisherMetadata(Session, PublisherId, LogFilePath, Locale, Flags);
	}
	api::EVT_HANDLE EvtCreateRenderContext(DWORD ValuePathsCount, LPCWSTR* ValuePaths, DWORD Flags) {
		if (!api::pEvtCreateRenderContext)
			throw nscp_exception("Failed to load: EvtCreateRenderContext");
		return api::pEvtCreateRenderContext(ValuePathsCount, ValuePaths, Flags);
	}
	BOOL EvtRender(api::EVT_HANDLE Context, api::EVT_HANDLE Fragment, DWORD Flags, DWORD BufferSize, PVOID Buffer, PDWORD BufferUsed, PDWORD PropertyCount) {
		if (!api::pEvtRender)
			throw nscp_exception("Failed to load: EvtRender");
		return api::pEvtRender(Context, Fragment, Flags, BufferSize, Buffer, BufferUsed, PropertyCount);
	}
	BOOL EvtNext(api::EVT_HANDLE ResultSet, DWORD EventsSize, api::PEVT_HANDLE Events, DWORD Timeout, DWORD Flags, PDWORD Returned) {
		if (!api::pEvtNext)
			throw nscp_exception("Failed to load: EvtNext");
		return api::pEvtNext(ResultSet, EventsSize, Events, Timeout, Flags, Returned);
	}
	BOOL EvtSeek(api::EVT_HANDLE ResultSet, LONGLONG Position, api::EVT_HANDLE Bookmark, DWORD Timeout, DWORD Flags) {
		if (!api::pEvtSeek)
			throw nscp_exception("Failed to load: EvtSeek");
		return api::pEvtSeek(ResultSet, Position, Bookmark, Timeout, Flags);
	}
	api::EVT_HANDLE EvtQuery(api::EVT_HANDLE Session, LPCWSTR Path, LPCWSTR Query, DWORD Flags) {
		if (!api::pEvtQuery)
			throw nscp_exception("Failed to load: EvtQuery");
		return api::pEvtQuery(Session, Path, Query, Flags);
	}
	api::EVT_HANDLE EvtOpenPublisherEnum(api::EVT_HANDLE Session, DWORD Flags) {
		if (!api::pEvtOpenPublisherEnum)
			throw nscp_exception("Failed to load: EvtOpenPublisherEnum");
		return api::pEvtOpenPublisherEnum(Session, Flags);
	}
	BOOL EvtClose(api::EVT_HANDLE Object) {
		if (!api::pEvtClose)
			throw nscp_exception("Failed to load: EvtClose");
		return api::pEvtClose(Object);
	}
	BOOL EvtNextPublisherId(api::EVT_HANDLE PublisherEnum, DWORD PublisherIdBufferSize, LPWSTR PublisherIdBuffer, PDWORD PublisherIdBufferUsed) {
		if (!api::ptEvtNextPublisherId)
			throw nscp_exception("Failed to load: EvtOpenPublisherEnum");
		return api::ptEvtNextPublisherId(PublisherEnum, PublisherIdBufferSize, PublisherIdBuffer, PublisherIdBufferUsed);
	}
	api::EVT_HANDLE EvtOpenChannelEnum(api::EVT_HANDLE Session, DWORD Flags) {
		if (!api::pEvtOpenChannelEnum)
			throw nscp_exception("Failed to load: EvtOpenChannelEnum");
		return api::pEvtOpenChannelEnum(Session, Flags);
	}
	BOOL EvtNextChannelPath(api::EVT_HANDLE PublisherEnum, DWORD PublisherIdBufferSize, LPWSTR PublisherIdBuffer, PDWORD PublisherIdBufferUsed) {
		if (!api::pEvtNextChannelPath)
			throw nscp_exception("Failed to load: EvtNextChannelPath");
		return api::pEvtNextChannelPath(PublisherEnum, PublisherIdBufferSize, PublisherIdBuffer, PublisherIdBufferUsed);
	}
	BOOL EvtGetPublisherMetadataProperty(api::EVT_HANDLE PublisherMetadata, api::EVT_PUBLISHER_METADATA_PROPERTY_ID PropertyId, DWORD Flags, DWORD PublisherMetadataPropertyBufferSize, api::PEVT_VARIANT PublisherMetadataPropertyBuffer, PDWORD PublisherMetadataPropertyBufferUsed) {
		if (!api::pEvtGetPublisherMetadataProperty)
			throw nscp_exception("Failed to load: EvtGetPublisherMetadataProperty");
		return api::pEvtGetPublisherMetadataProperty(PublisherMetadata, PropertyId, Flags, PublisherMetadataPropertyBufferSize, PublisherMetadataPropertyBuffer, PublisherMetadataPropertyBufferUsed);
	}
	BOOL EvtGetObjectArrayProperty(api::EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray, DWORD PropertyId, DWORD ArrayIndex, DWORD Flags, DWORD PropertyValueBufferSize, api::PEVT_VARIANT PropertyValueBuffer, PDWORD PropertyValueBufferUsed) {
		if (!api::pEvtGetObjectArrayProperty)
			throw nscp_exception("Failed to load: EvtGetObjectArrayProperty");
		return api::pEvtGetObjectArrayProperty(ObjectArray, PropertyId, ArrayIndex, Flags, PropertyValueBufferSize, PropertyValueBuffer, PropertyValueBufferUsed);

	}
	BOOL EvtGetObjectArraySize(api::EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray, PDWORD ObjectArraySize) {
		if (!api::pEvtGetObjectArraySize)
			throw nscp_exception("Failed to load: EvtGetObjectArraySize");
		return api::pEvtGetObjectArraySize(ObjectArray, ObjectArraySize);

	}

	long long get_int(const eventlog::api::PEVT_VARIANT &var) {
		using namespace eventlog::api;
		if (var->Type == EvtVarTypeNull)
			return 0;
		if (var->Type == EvtVarTypeSByte)
			return var->SByteVal;
		if (var->Type == EvtVarTypeByte)
			return var->ByteVal;
		if (var->Type == EvtVarTypeInt16)
			return var->Int16Val;
		if (var->Type == EvtVarTypeUInt16)
			return var->UInt16Val;
		if (var->Type == EvtVarTypeInt32)
			return var->Int32Val;
		if (var->Type == EvtVarTypeUInt32)
			return var->UInt32Val;
		if (var->Type == EvtVarTypeInt64)
			return var->Int64Val;
		if (var->Type == EvtVarTypeUInt64)
			return var->UInt64Val;
		if (var->Type == EvtVarTypeHexInt32)
			return var->Int32Val;
		if (var->Type == EvtVarTypeHexInt64)
			return var->Int64Val;
		return 0;
	}

	std::string get_str(const eventlog::api::PEVT_VARIANT &var) {
		using namespace eventlog::api;
		if (var->Type == EvtVarTypeNull)
			return "";
		if (var->Type == EvtVarTypeAnsiString)
			return var->AnsiStringVal;
		if (var->Type == EvtVarTypeString)
			return utf8::cvt<std::string>(var->StringVal);
		return strEx::s::xtos(get_int(var));
	}


	eventlog_table fetch_table(api::EVT_HANDLE PublisherMetadata, api::EVT_PUBLISHER_METADATA_PROPERTY_ID PropertyId, DWORD KeyPropertyId, DWORD ValuePropertyId) {
		eventlog_table ret;
		DWORD dwBufferSize = 0;
		DWORD status = 0;
		if (eventlog::EvtGetPublisherMetadataProperty(PublisherMetadata, PropertyId, 0, 0, NULL, &dwBufferSize))
			return ret;
		hlp::buffer<wchar_t, eventlog::api::PEVT_VARIANT> buffer1(dwBufferSize+10);

		if (!eventlog::EvtGetPublisherMetadataProperty(PublisherMetadata, PropertyId, 0, buffer1.size(), buffer1.get(), &dwBufferSize)) {
			return ret;
		}
		eventlog::api::PEVT_VARIANT var1 = buffer1.get();
		DWORD dwArraySize = 0;

		if (!eventlog::EvtGetObjectArraySize(var1->EvtHandleVal, &dwArraySize)) {
			return ret;
		}
		hlp::buffer<wchar_t, eventlog::api::PEVT_VARIANT> buffer2(4096);
		for (int i = 0; i < dwArraySize; i++) {
			if (!eventlog::EvtGetObjectArrayProperty(var1->EvtHandleVal, KeyPropertyId, i, 0, buffer2.size(), buffer2.get(), &dwBufferSize)) {
				status = GetLastError();
				if (status == ERROR_INSUFFICIENT_BUFFER) {
					buffer2.resize(dwBufferSize);
					if (!eventlog::EvtGetObjectArrayProperty(var1->EvtHandleVal, KeyPropertyId, i, 0, buffer2.size(), buffer2.get(), &dwBufferSize)) {
						return ret;
					}
				} else {
					return ret;
				}
			} 
			eventlog::api::PEVT_VARIANT varKey = buffer2.get();
			long long key = get_int(varKey);
			
			if (!eventlog::EvtGetObjectArrayProperty(var1->EvtHandleVal, ValuePropertyId, i, 0, buffer2.size(), buffer2.get(), &dwBufferSize)) {
				status = GetLastError();
				if (status == ERROR_INSUFFICIENT_BUFFER) {
					buffer2.resize(dwBufferSize);
					if (!eventlog::EvtGetObjectArrayProperty(var1->EvtHandleVal, ValuePropertyId, i, 0, buffer2.size(), buffer2.get(), &dwBufferSize)) {
						return ret;
					}
				} else {
					return ret;
				}
			}
			eventlog::api::PEVT_VARIANT varVal = buffer2.get();
			ret[key] =get_str(varVal);
		}
		return ret;
	}


}