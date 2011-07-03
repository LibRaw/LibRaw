#pragma once
// LibRawWindows.h
//
// Windows specific classes and routines for LibRaw
//
// Copyright(c) 2011 Linc Brookes

// LibRawApp is free software. The source code, documentation and artwork are distributed under the terms of the 
// GNU LESSER GENERAL PUBLIC LICENSE version 2.1
//   see http://www.gnu.org/licenses/lgpl-2.1.html for details
//
// LibRawApp requires the LibRaw library
// Information and source code are available here: http://www.libraw.org/

//#include <windows.h>
//#include "../libraw/libraw_datastream.h"

//#include <stdint.h>
#include <exception>

// class LibRaw_windows_datastream 
// a windows file mapping encapsulated in a LibRaw_buffer_datastream
// against LibRaw convention, this class may throw a std::runtime_exception
class LibRaw_windows_datastream : public LibRaw_buffer_datastream 
{
public:
	// ctor: high level constructor opens a file by name
	LibRaw_windows_datastream(const TCHAR* sFile)
		: LibRaw_buffer_datastream(NULL, 0)
		, hMap_(0)
		, pView_(NULL)
	{
		HANDLE hFile = CreateFile(sFile, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
		if (hFile == INVALID_HANDLE_VALUE) 
			throw std::runtime_error("failed to open the file"); 

		try { Open(hFile); 	}	catch(...) { CloseHandle(hFile); throw; }

		CloseHandle(hFile);		// windows will defer the actual closing of this handle until the hMap_ is closed
		reconstruct_base();
	}

	// ctor: construct with a file handle - caller is responsible for closing the file handle
	LibRaw_windows_datastream(HANDLE hFile)
		: LibRaw_buffer_datastream(NULL, 0)
		, hMap_(0)
		, pView_(NULL)
	{
		Open(hFile);
		reconstruct_base();
	}

	// dtor: unmap and close the mapping handle
	virtual ~LibRaw_windows_datastream()
	{
		if (pView_ != NULL)
			::UnmapViewOfFile(pView_);

		if (hMap_ != 0)
			::CloseHandle(hMap_);
	}

protected:
	void Open(HANDLE hFile)
	{
		// create a file mapping handle on the file handle
		hMap_ = ::CreateFileMapping(hFile, 0, PAGE_READONLY, 0, 0, 0);
		if (hMap_ == NULL)	throw std::runtime_error("failed to create file mapping"); 

		// now map the whole file base view
		if (!::GetFileSizeEx(hFile, (PLARGE_INTEGER)&cbView_))
			throw std::runtime_error("failed to get the file size"); 

		pView_ = ::MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, (size_t)cbView_);
		if (pView_ == NULL)	
			throw std::runtime_error("failed to map the file"); 
	}

	inline void reconstruct_base()
	{
		// this subterfuge is to overcome the private-ness of LibRaw_buffer_datastream
		(LibRaw_buffer_datastream&)*this = LibRaw_buffer_datastream(pView_, (size_t)cbView_);
	}

	HANDLE		hMap_;			// handle of the file mapping
	void*		pView_;			// pointer to the mapped memory
//	uint64_t	cbView_;		// size of the mapping in bytes
	__int64	cbView_;		// size of the mapping in bytes
};


