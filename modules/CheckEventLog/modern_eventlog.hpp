/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <handle.hpp>
#include <Windows.h>

#include <map>

namespace eventlog {

	/// severity levels
#define WINEVENT_AUDIT_TYPE            0
#define WINEVENT_CRITICAL_TYPE         1
#define WINEVENT_ERROR_TYPE            2
#define WINEVENT_WARNING_TYPE          3
#define WINEVENT_INFORMATION_TYPE      4
#define WINEVENT_VERBOSE_TYPE          5

#define WINEVENT_AUDIT_FAILURE 0x10000000000000LL
#define WINEVENT_AUDIT_SUCCESS 0x20000000000000LL

	namespace api {
		typedef HANDLE EVT_HANDLE, *PEVT_HANDLE;
		typedef HANDLE EVT_OBJECT_ARRAY_PROPERTY_HANDLE;

		typedef enum _EVT_VARIANT_TYPE {
			EvtVarTypeNull = 0,
			EvtVarTypeString = 1,
			EvtVarTypeAnsiString = 2,
			EvtVarTypeSByte = 3,
			EvtVarTypeByte = 4,
			EvtVarTypeInt16 = 5,
			EvtVarTypeUInt16 = 6,
			EvtVarTypeInt32 = 7,
			EvtVarTypeUInt32 = 8,
			EvtVarTypeInt64 = 9,
			EvtVarTypeUInt64 = 10,
			EvtVarTypeSingle = 11,
			EvtVarTypeDouble = 12,
			EvtVarTypeBoolean = 13,
			EvtVarTypeBinary = 14,
			EvtVarTypeGuid = 15,
			EvtVarTypeSizeT = 16,
			EvtVarTypeFileTime = 17,
			EvtVarTypeSysTime = 18,
			EvtVarTypeSid = 19,
			EvtVarTypeHexInt32 = 20,
			EvtVarTypeHexInt64 = 21,

			// these types used internally
			EvtVarTypeEvtHandle = 32,
			EvtVarTypeEvtXml = 35
		} EVT_VARIANT_TYPE;

#define EVT_VARIANT_TYPE_MASK 0x7f
#define EVT_VARIANT_TYPE_ARRAY 128

		typedef struct _EVT_VARIANT {
			union {
				BOOL        BooleanVal;
				INT8        SByteVal;
				INT16       Int16Val;
				INT32       Int32Val;
				INT64       Int64Val;
				UINT8       ByteVal;
				UINT16      UInt16Val;
				UINT32      UInt32Val;
				UINT64      UInt64Val;
				float       SingleVal;
				double      DoubleVal;
				ULONGLONG   FileTimeVal;
				SYSTEMTIME* SysTimeVal;
				GUID*       GuidVal;
				LPCWSTR     StringVal;
				LPCSTR      AnsiStringVal;
				PBYTE       BinaryVal;
				PSID        SidVal;
				size_t      SizeTVal;

				// array fields
				BOOL*       BooleanArr;
				INT8*       SByteArr;
				INT16*      Int16Arr;
				INT32*      Int32Arr;
				INT64*      Int64Arr;
				UINT8*      ByteArr;
				UINT16*     UInt16Arr;
				UINT32*     UInt32Arr;
				UINT64*     UInt64Arr;
				float*      SingleArr;
				double*     DoubleArr;
				FILETIME*   FileTimeArr;
				SYSTEMTIME* SysTimeArr;
				GUID*       GuidArr;
				LPWSTR*     StringArr;
				LPSTR*      AnsiStringArr;
				PSID*       SidArr;
				size_t*     SizeTArr;

				// internal fields
				EVT_HANDLE  EvtHandleVal;
				LPCWSTR     XmlVal;
				LPCWSTR*    XmlValArr;
			};

			DWORD Count;   // number of elements (not length) in bytes.
			DWORD Type;
		} EVT_VARIANT, *PEVT_VARIANT;

		typedef enum _EVT_SUBSCRIBE_FLAGS {
			EvtSubscribeToFutureEvents = 1,
			EvtSubscribeStartAtOldestRecord = 2,
			EvtSubscribeStartAfterBookmark = 3,
			EvtSubscribeOriginMask = 3,

			EvtSubscribeTolerateQueryErrors = 0x1000,

			EvtSubscribeStrict = 0x10000,

		} EVT_SUBSCRIBE_FLAGS;

		typedef enum _EVT_QUERY_FLAGS {
			EvtQueryChannelPath = 0x1,
			EvtQueryFilePath = 0x2,

			EvtQueryForwardDirection = 0x100,
			EvtQueryReverseDirection = 0x200,

			EvtQueryTolerateQueryErrors = 0x1000
		} EVT_QUERY_FLAGS;

		typedef enum _EVT_SEEK_FLAGS {
			EvtSeekRelativeToFirst = 1,
			EvtSeekRelativeToLast = 2,
			EvtSeekRelativeToCurrent = 3,
			EvtSeekRelativeToBookmark = 4,
			EvtSeekOriginMask = 7,

			EvtSeekStrict = 0x10000,
		} EVT_SEEK_FLAGS;

		typedef enum _EVT_SYSTEM_PROPERTY_ID {
			EvtSystemProviderName = 0,          // EvtVarTypeString
			EvtSystemProviderGuid,              // EvtVarTypeGuid
			EvtSystemEventID,                   // EvtVarTypeUInt16
			EvtSystemQualifiers,                // EvtVarTypeUInt16
			EvtSystemLevel,                     // EvtVarTypeUInt8
			EvtSystemTask,                      // EvtVarTypeUInt16
			EvtSystemOpcode,                    // EvtVarTypeUInt8
			EvtSystemKeywords,                  // EvtVarTypeHexInt64
			EvtSystemTimeCreated,               // EvtVarTypeFileTime
			EvtSystemEventRecordId,             // EvtVarTypeUInt64
			EvtSystemActivityID,                // EvtVarTypeGuid
			EvtSystemRelatedActivityID,         // EvtVarTypeGuid
			EvtSystemProcessID,                 // EvtVarTypeUInt32
			EvtSystemThreadID,                  // EvtVarTypeUInt32
			EvtSystemChannel,                   // EvtVarTypeString
			EvtSystemComputer,                  // EvtVarTypeString
			EvtSystemUserID,                    // EvtVarTypeSid
			EvtSystemVersion,                   // EvtVarTypeUInt8
			EvtSystemPropertyIdEND
		} EVT_SYSTEM_PROPERTY_ID;

		typedef enum _EVT_RENDER_CONTEXT_FLAGS {
			EvtRenderContextValues = 0,         // Render specific properties
			EvtRenderContextSystem,             // Render all system properties (System)
			EvtRenderContextUser                // Render all user properties (User/EventData)
		} EVT_RENDER_CONTEXT_FLAGS;

		typedef enum _EVT_RENDER_FLAGS {
			EvtRenderEventValues = 0,           // Variants
			EvtRenderEventXml,                  // XML
			EvtRenderBookmark                   // Bookmark
		} EVT_RENDER_FLAGS;

		typedef enum _EVT_FORMAT_MESSAGE_FLAGS {
			EvtFormatMessageEvent = 1,
			EvtFormatMessageLevel,
			EvtFormatMessageTask,
			EvtFormatMessageOpcode,
			EvtFormatMessageKeyword,
			EvtFormatMessageChannel,
			EvtFormatMessageProvider,
			EvtFormatMessageId,
			EvtFormatMessageXml,
		} EVT_FORMAT_MESSAGE_FLAGS;


		typedef enum _EVT_PUBLISHER_METADATA_PROPERTY_ID {
			EvtPublisherMetadataPublisherGuid = 0,      // EvtVarTypeGuid
			EvtPublisherMetadataResourceFilePath,       // EvtVarTypeString
			EvtPublisherMetadataParameterFilePath,      // EvtVarTypeString
			EvtPublisherMetadataMessageFilePath,        // EvtVarTypeString
			EvtPublisherMetadataHelpLink,               // EvtVarTypeString
			EvtPublisherMetadataPublisherMessageID,     // EvtVarTypeUInt32

			EvtPublisherMetadataChannelReferences,      // EvtVarTypeEvtHandle, ObjectArray
			EvtPublisherMetadataChannelReferencePath,   // EvtVarTypeString
			EvtPublisherMetadataChannelReferenceIndex,  // EvtVarTypeUInt32
			EvtPublisherMetadataChannelReferenceID,     // EvtVarTypeUInt32
			EvtPublisherMetadataChannelReferenceFlags,  // EvtVarTypeUInt32
			EvtPublisherMetadataChannelReferenceMessageID, // EvtVarTypeUInt32

			EvtPublisherMetadataLevels,                 // EvtVarTypeEvtHandle, ObjectArray
			EvtPublisherMetadataLevelName,              // EvtVarTypeString
			EvtPublisherMetadataLevelValue,             // EvtVarTypeUInt32
			EvtPublisherMetadataLevelMessageID,         // EvtVarTypeUInt32

			EvtPublisherMetadataTasks,                  // EvtVarTypeEvtHandle, ObjectArray
			EvtPublisherMetadataTaskName,               // EvtVarTypeString
			EvtPublisherMetadataTaskEventGuid,          // EvtVarTypeGuid
			EvtPublisherMetadataTaskValue,              // EvtVarTypeUInt32
			EvtPublisherMetadataTaskMessageID,          // EvtVarTypeUInt32

			EvtPublisherMetadataOpcodes,                // EvtVarTypeEvtHandle, ObjectArray
			EvtPublisherMetadataOpcodeName,             // EvtVarTypeString
			EvtPublisherMetadataOpcodeValue,            // EvtVarTypeUInt32
			EvtPublisherMetadataOpcodeMessageID,        // EvtVarTypeUInt32

			EvtPublisherMetadataKeywords,               // EvtVarTypeEvtHandle, ObjectArray
			EvtPublisherMetadataKeywordName,            // EvtVarTypeString
			EvtPublisherMetadataKeywordValue,           // EvtVarTypeUInt64
			EvtPublisherMetadataKeywordMessageID,       // EvtVarTypeUInt32


			EvtPublisherMetadataPropertyIdEND

		} EVT_PUBLISHER_METADATA_PROPERTY_ID;


		typedef EVT_HANDLE(WINAPI *tEvtOpenPublisherEnum)(
			EVT_HANDLE Session,
			DWORD Flags
			);

		typedef BOOL(WINAPI *tEvtNextPublisherId)(
			EVT_HANDLE PublisherEnum,
			DWORD PublisherIdBufferSize,
			_Out_writes_to_opt_(PublisherIdBufferSize, *PublisherIdBufferUsed)
			LPWSTR PublisherIdBuffer,
			_Out_ PDWORD PublisherIdBufferUsed
			);

		typedef EVT_HANDLE(WINAPI *tEvtOpenChannelEnum)(
			_In_  EVT_HANDLE Session,
			_In_  DWORD Flags
			);
		typedef BOOL(WINAPI *tEvtNextChannelPath)(
			_In_   EVT_HANDLE ChannelEnum,
			_In_   DWORD ChannelPathBufferSize,
			_In_   LPWSTR ChannelPathBuffer,
			_Out_  PDWORD ChannelPathBufferUsed
			);
		typedef BOOL(WINAPI *tEvtClose)(
			_In_  EVT_HANDLE Object
			);
		typedef EVT_HANDLE(WINAPI *tEvtQuery)(
			EVT_HANDLE Session,
			LPCWSTR Path,
			LPCWSTR Query,
			DWORD Flags
			);
		typedef BOOL(WINAPI *tEvtNext)(
			EVT_HANDLE ResultSet,
			DWORD EventsSize,
			PEVT_HANDLE Events,
			DWORD Timeout,
			DWORD Flags,
			_Out_ PDWORD Returned
			);

		typedef BOOL(WINAPI *tEvtSeek)(
			EVT_HANDLE ResultSet,
			LONGLONG Position,
			EVT_HANDLE Bookmark,
			_Reserved_ DWORD Timeout,           // currently must be 0
			DWORD Flags
			);

		typedef EVT_HANDLE(WINAPI *tEvtCreateRenderContext)(
			DWORD ValuePathsCount,
			_In_reads_opt_(ValuePathsCount) LPCWSTR* ValuePaths,
			DWORD Flags                         // EVT_RENDER_CONTEXT_FLAGS
			);

		typedef BOOL(WINAPI *tEvtRender)(
			EVT_HANDLE Context,
			EVT_HANDLE Fragment,
			DWORD Flags,                        // EVT_RENDER_FLAGS
			DWORD BufferSize,
			_Out_writes_bytes_to_opt_(BufferSize, *BufferUsed) PVOID Buffer,
			_Out_ PDWORD BufferUsed,
			_Out_ PDWORD PropertyCount
			);

		typedef EVT_HANDLE(WINAPI *tEvtOpenPublisherMetadata)(
			EVT_HANDLE Session,
			LPCWSTR PublisherId,
			LPCWSTR LogFilePath,
			LCID Locale,
			DWORD Flags
			);

		typedef BOOL(WINAPI *tEvtFormatMessage)(
			EVT_HANDLE PublisherMetadata,       // Except for forwarded events
			EVT_HANDLE Event,
			DWORD MessageId,
			DWORD ValueCount,
			PEVT_VARIANT Values,
			DWORD Flags,
			DWORD BufferSize,
			_Out_writes_to_opt_(BufferSize, *BufferUsed) LPWSTR Buffer,
			_Out_ PDWORD BufferUsed
			);

		typedef BOOL(WINAPI *tEvtGetPublisherMetadataProperty)(
			EVT_HANDLE PublisherMetadata,
			EVT_PUBLISHER_METADATA_PROPERTY_ID PropertyId,
			DWORD Flags,
			DWORD PublisherMetadataPropertyBufferSize,
			PEVT_VARIANT PublisherMetadataPropertyBuffer,
			_Out_ PDWORD PublisherMetadataPropertyBufferUsed
			);

		typedef BOOL (WINAPI *tEvtGetObjectArrayProperty)(
			EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray,
			DWORD PropertyId,
			DWORD ArrayIndex,
			DWORD Flags,
			DWORD PropertyValueBufferSize,
			PEVT_VARIANT PropertyValueBuffer,
			_Out_ PDWORD PropertyValueBufferUsed
			);

		typedef BOOL (*tEvtGetObjectArraySize)(
			EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray,
			_Out_ PDWORD ObjectArraySize
			);

		typedef enum _EVT_SUBSCRIBE_NOTIFY_ACTION {
			EvtSubscribeActionError = 0,
			EvtSubscribeActionDeliver

		} EVT_SUBSCRIBE_NOTIFY_ACTION;

		typedef DWORD(WINAPI *EVT_SUBSCRIBE_CALLBACK)(
			EVT_SUBSCRIBE_NOTIFY_ACTION Action,
			PVOID UserContext,
			EVT_HANDLE Event);

		typedef EVT_HANDLE (WINAPI *tEvtSubscribe)(
			EVT_HANDLE Session,
			HANDLE SignalEvent,
			LPCWSTR ChannelPath,
			LPCWSTR Query,
			EVT_HANDLE Bookmark,
			PVOID context,
			EVT_SUBSCRIBE_CALLBACK Callback,
			DWORD Flags
			);

		typedef EVT_HANDLE (WINAPI *tEvtCreateBookmark)(
			LPCWSTR BookmarkXml
			);
		typedef BOOL (WINAPI *tEvtUpdateBookmark)(
			EVT_HANDLE Bookmark,
			EVT_HANDLE Event
		);



		void load_procs();
		bool supports_modern();
	}
	BOOL EvtFormatMessage(api::EVT_HANDLE PublisherMetadata, api::EVT_HANDLE Event, DWORD MessageId, DWORD ValueCount, api::PEVT_VARIANT Values, DWORD Flags, DWORD BufferSize, LPWSTR Buffer, PDWORD BufferUsed);
	int EvtFormatMessage(api::EVT_HANDLE PublisherMetadata, api::EVT_HANDLE Event, DWORD MessageId, DWORD ValueCount, api::PEVT_VARIANT Values, DWORD Flags, std::string &str);
	api::EVT_HANDLE EvtOpenPublisherMetadata(api::EVT_HANDLE Session, LPCWSTR PublisherId, LPCWSTR LogFilePath, LCID Locale, DWORD Flags);
	api::EVT_HANDLE EvtCreateRenderContext(DWORD ValuePathsCount, LPCWSTR* ValuePaths, DWORD Flags);
	BOOL EvtRender(api::EVT_HANDLE Context, api::EVT_HANDLE Fragment, DWORD Flags, DWORD BufferSize, PVOID Buffer, PDWORD BufferUsed, PDWORD PropertyCount);
	BOOL EvtNext(api::EVT_HANDLE ResultSet, DWORD EventsSize, api::PEVT_HANDLE Events, DWORD Timeout, DWORD Flags, PDWORD Returned);
	BOOL EvtSeek(api::EVT_HANDLE ResultSet, LONGLONG Position, api::EVT_HANDLE Bookmark, DWORD Timeout, DWORD Flags);
	api::EVT_HANDLE EvtQuery(api::EVT_HANDLE Session, LPCWSTR Path, LPCWSTR Query, DWORD Flags);
	api::EVT_HANDLE EvtOpenPublisherEnum(api::EVT_HANDLE Session, DWORD Flags);
	BOOL EvtClose(api::EVT_HANDLE Object);
	BOOL EvtNextPublisherId(api::EVT_HANDLE PublisherEnum, DWORD PublisherIdBufferSize, LPWSTR PublisherIdBuffer, PDWORD PublisherIdBufferUsed);
	api::EVT_HANDLE EvtOpenChannelEnum(api::EVT_HANDLE Session, DWORD Flags);
	BOOL EvtNextChannelPath(api::EVT_HANDLE PublisherEnum, DWORD PublisherIdBufferSize, LPWSTR PublisherIdBuffer, PDWORD PublisherIdBufferUsed);
	api::EVT_HANDLE EvtCreateBookmark(LPCWSTR BookmarkXml);
	BOOL EvtUpdateBookmark(api::EVT_HANDLE Bookmark, api::EVT_HANDLE Event);


	BOOL EvtGetPublisherMetadataProperty(api::EVT_HANDLE PublisherMetadata, api::EVT_PUBLISHER_METADATA_PROPERTY_ID PropertyId, DWORD Flags, DWORD PublisherMetadataPropertyBufferSize, api::PEVT_VARIANT PublisherMetadataPropertyBuffer, PDWORD PublisherMetadataPropertyBufferUsed);
	BOOL EvtGetObjectArrayProperty(api::EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray, DWORD PropertyId, DWORD ArrayIndex, DWORD Flags, DWORD PropertyValueBufferSize, api::PEVT_VARIANT PropertyValueBuffer, PDWORD PropertyValueBufferUsed);
	BOOL EvtGetObjectArraySize(api::EVT_OBJECT_ARRAY_PROPERTY_HANDLE ObjectArray, PDWORD ObjectArraySize);

	api::EVT_HANDLE EvtSubscribe(api::EVT_HANDLE Session, HANDLE SignalEvent, LPCWSTR ChannelPath, LPCWSTR Query, api::EVT_HANDLE Bookmark, PVOID context, api::EVT_SUBSCRIBE_CALLBACK Callback, DWORD Flags);


	typedef std::map<long long, std::string> eventlog_table;
	eventlog_table fetch_table(api::EVT_HANDLE PublisherMetadata, api::EVT_PUBLISHER_METADATA_PROPERTY_ID PropertyId, DWORD KeyPropertyId, DWORD ValuePropertyId);


	struct evt_closer {
		static void close(api::EVT_HANDLE &handle) {
			EvtClose(handle);
		}
	};
	typedef hlp::handle<api::EVT_HANDLE, evt_closer> evt_handle;
}