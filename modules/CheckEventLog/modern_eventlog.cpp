#pragma once

#include <error.hpp>

#include "modern_eventlog.hpp"

namespace eventlog {
	namespace api {
		tEvtOpenPublisherEnum pEvtOpenPublisherEnum = NULL;
		tEvtClose pEvtClose = NULL;
		tEvtNextPublisherId ptEvtNextPublisherId = NULL;
		tEvtOpenChannelEnum pEvtOpenChannelEnum = NULL;
		tEvtNextChannelPath pEvtNextChannelPath = NULL;
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
}