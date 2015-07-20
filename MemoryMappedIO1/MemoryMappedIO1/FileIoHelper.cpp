/**----------------------------------------------------------------------------
* FileIoHelperClass.cpp
*-----------------------------------------------------------------------------
*
*-----------------------------------------------------------------------------
* All rights reserved by somma (fixbrain@gmail.com, unsorted@msn.com)
*-----------------------------------------------------------------------------
* 13:10:2011   17:04 created
**---------------------------------------------------------------------------*/
#include "stdafx.h"



#include "FileIoHelper.h"





/**----------------------------------------------------------------------------
\brief

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
FileIoHelper::FileIoHelper()
	: mReadOnly(TRUE),
	mFileHandle(INVALID_HANDLE_VALUE),
	mFileMap(NULL),
	mFileView(NULL)
{
	mFileSize.QuadPart = 0;
}

/**----------------------------------------------------------------------------
\brief

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
FileIoHelper::~FileIoHelper()
{
	this->FIOClose();
}


bool IsFileExistW(_In_ const wchar_t* file_path)
{
	_ASSERTE(NULL != file_path);
	_ASSERTE(TRUE != IsBadStringPtrW(file_path, MAX_PATH));
	if ((NULL == file_path) || (TRUE == IsBadStringPtrW(file_path, MAX_PATH))) return false;

	WIN32_FILE_ATTRIBUTE_DATA info = { 0 };

	if (GetFileAttributesExW(file_path, GetFileExInfoStandard, &info) == 0)
		return false;
	else
		return true;
}

/**----------------------------------------------------------------------------
\brief  파일 IO 를 위해 파일을 오픈한다.

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
DTSTATUS
FileIoHelper::FIOpenForRead(IN std::wstring FilePath)
{
	if (TRUE == Initialized()) { FIOClose(); }

	mReadOnly = TRUE;
	if (TRUE != IsFileExistW(FilePath.c_str()))
	{

			return DTS_NO_FILE_EXIST;
	}

#pragma warning(disable: 4127)
	DTSTATUS status = DTS_WINAPI_FAILED;
	do
	{
		mFileHandle = CreateFileW(
			FilePath.c_str(),
			GENERIC_READ,
			NULL,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);
		if (INVALID_HANDLE_VALUE == mFileHandle)
		{

				break;
		}

		// check file size 
		// 
		if (TRUE != GetFileSizeEx(mFileHandle, &mFileSize))
		{

				break;
		}

		mFileMap = CreateFileMapping(
			mFileHandle,
			NULL,
			PAGE_READONLY,
			0,
			0,
			NULL
			);
		if (NULL == mFileMap)
		{

				break;
		}

		status = DTS_SUCCESS;
	} while (FALSE);
#pragma warning(default: 4127)

	if (TRUE != DT_SUCCEEDED(status))
	{
		if (INVALID_HANDLE_VALUE != mFileHandle)
		{
			CloseHandle(mFileHandle);
			mFileHandle = INVALID_HANDLE_VALUE;
		}
		if (NULL != mFileMap) CloseHandle(mFileMap);
	}

	return status;
}

/**----------------------------------------------------------------------------
\brief		FileSize 바이트 짜리 파일을 생성한다.

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
DTSTATUS
FileIoHelper::FIOCreateFile(
IN std::wstring FilePath,
IN LARGE_INTEGER FileSize
)
{
	if (TRUE == Initialized()) { FIOClose(); }
	if (FileSize.QuadPart == 0) return DTS_INVALID_PARAMETER;

	mReadOnly = FALSE;

#pragma warning(disable: 4127)
	DTSTATUS status = DTS_WINAPI_FAILED;
	do
	{
		mFileSize = FileSize;

		mFileHandle = CreateFileW(
			FilePath.c_str(),
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ,		// write 도중 다른 프로세스에서 읽기가 가능
			NULL,
			CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL
			);
		if (INVALID_HANDLE_VALUE == mFileHandle)
		{
			printf("HANDLE error\n");
				break;
		}

		// increase file size
		// 
		if (TRUE != SetFilePointerEx(mFileHandle, mFileSize, NULL, FILE_BEGIN))
		{

			printf("FilePointer Error\n");
				break;
		}

		if (TRUE != SetEndOfFile(mFileHandle))
		{
			printf("EndofFile Error\n");
			break;
		}

		mFileMap = CreateFileMapping(
			mFileHandle,
			NULL,
			PAGE_READWRITE,
			0,
			0,
			NULL
			);
		if (NULL == mFileMap)
		{
			printf("FileMap Error\n");
				break;
		}

		status = DTS_SUCCESS;
	} while (FALSE);
#pragma warning(default: 4127)

	if (TRUE != DT_SUCCEEDED(status))
	{
		if (INVALID_HANDLE_VALUE != mFileHandle)
		{
			CloseHandle(mFileHandle);
			mFileHandle = INVALID_HANDLE_VALUE;
		}
		if (NULL != mFileMap) CloseHandle(mFileMap);
	}

	CloseHandle(mFileHandle);
	return status;
}

/**----------------------------------------------------------------------------
\brief

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
void
FileIoHelper::FIOClose(
)
{
	if (TRUE != Initialized()) return;

	FIOUnreference();
	CloseHandle(mFileMap); mFileMap = NULL;
	CloseHandle(mFileHandle); mFileHandle = INVALID_HANDLE_VALUE;
}

/**----------------------------------------------------------------------------
\brief

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
DTSTATUS
FileIoHelper::FIOReference(
IN BOOL ReadOnly,
IN LARGE_INTEGER Offset,
IN DWORD Size,
IN OUT PUCHAR& ReferencedPointer
)
{
	if (TRUE != Initialized()) return DTS_INVALID_OBJECT_STATUS;
	if (TRUE == IsReadOnly())
	{
		if (TRUE != ReadOnly)
		{

				return DTS_INVALID_PARAMETER;
		}
	}

	_ASSERTE(NULL == mFileView);
	FIOUnreference();

	if (Offset.QuadPart + Size > mFileSize.QuadPart)
	{

			return DTS_INVALID_PARAMETER;
	}

	//
	// MapViewOfFile() 함수의 dwFileOffsetLow 파라미터는 
	// SYSTEM_INFO::dwAllocationGranularity 값의 배수이어야 한다.
	// 
	static DWORD AllocationGranularity = 0;
	if (0 == AllocationGranularity)
	{
		SYSTEM_INFO si = { 0 };
		GetSystemInfo(&si);
		AllocationGranularity = si.dwAllocationGranularity;
	}

	DWORD AdjustMask = AllocationGranularity - 1;
	LARGE_INTEGER AdjustOffset = { 0 };
	AdjustOffset.HighPart = Offset.HighPart;

	// AllocationGranularity 이하의 값을 버림
	// 
	AdjustOffset.LowPart = (Offset.LowPart & ~AdjustMask);

	// 버려진 값만큼 매핑할 사이즈를 증가
	// 
	DWORD BytesToMap = (Offset.LowPart & AdjustMask) + Size;

	mFileView = (PUCHAR)MapViewOfFile(
		mFileMap,
		(TRUE == ReadOnly) ? FILE_MAP_READ : FILE_MAP_READ | FILE_MAP_WRITE,
		AdjustOffset.HighPart,
		AdjustOffset.LowPart,
		BytesToMap
		);
	if (NULL == mFileView)
	{

			return DTS_WINAPI_FAILED;
	}

	ReferencedPointer = &mFileView[Offset.LowPart & AdjustMask];
	return DTS_SUCCESS;
}

/**----------------------------------------------------------------------------
\brief

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
void
FileIoHelper::FIOUnreference()
{
	if (NULL != mFileView)
	{
		UnmapViewOfFile(mFileView);
		mFileView = NULL;
	}
}

/**----------------------------------------------------------------------------
\brief

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
DTSTATUS
FileIoHelper::FIOReadFromFile(
IN LARGE_INTEGER Offset,
IN DWORD Size,
IN OUT PUCHAR Buffer
)
{
	_ASSERTE(NULL != Buffer);
	if (NULL == Buffer) return DTS_INVALID_PARAMETER;

	UCHAR* p = NULL;
	DTSTATUS status = FIOReference(TRUE, Offset, Size, p);
	if (TRUE != DT_SUCCEEDED(status))
	{

			return status;
	}

	__try
	{
		RtlCopyMemory(Buffer, p, Size);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{

			status = DTS_EXCEPTION_RAISED;
	}

	FIOUnreference();
	return status;
}

/**----------------------------------------------------------------------------
\brief

\param
\return
\code

\endcode
-----------------------------------------------------------------------------*/
DTSTATUS
FileIoHelper::FIOWriteToFile(
IN LARGE_INTEGER Offset,
IN DWORD Size,
IN PUCHAR Buffer
)
{
	_ASSERTE(NULL != Buffer);
	_ASSERTE(0 != Size);
	_ASSERTE(NULL != Buffer);
	if (NULL == Buffer || 0 == Size || NULL == Buffer) return DTS_INVALID_PARAMETER;

	UCHAR* p = NULL;
	DTSTATUS status = FIOReference(FALSE, Offset, Size, p);
	if (TRUE != DT_SUCCEEDED(status))
	{

			return status;
	}

	__try
	{
		RtlCopyMemory(p, Buffer, Size);
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{

			status = DTS_EXCEPTION_RAISED;
	}

	FIOUnreference();
	return status;
}