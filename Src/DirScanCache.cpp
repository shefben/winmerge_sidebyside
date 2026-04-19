/**
 *  @file DirScanCache.cpp
 *
 *  @brief Implementation of DirScanCache — binary DIFFITEM tree serialization
 *
 *  Cache format (version 1):
 *    Header:
 *      magic       4 bytes  "WMDC"
 *      version     uint32   1
 *      leftPath    length-prefixed wstring
 *      rightPath   length-prefixed wstring
 *      compareMethod int32
 *      recursive   uint8
 *      leftRootMtime  int64  (FILETIME of left root dir)
 *      rightRootMtime int64  (FILETIME of right root dir)
 *      itemCount   uint32
 *    Items (depth-first pre-order):
 *      depth       uint16
 *      diffcode    uint32
 *      customFlags uint32
 *      nsdiffs     int32
 *      nidiffs     int32
 *      For each side (0, 1):
 *        path      length-prefixed wstring
 *        filename  length-prefixed wstring
 *        mtime     int64
 *        ctime     int64
 *        size      int64
 *        attributes uint32
 */

#include "pch.h"
#include "DirScanCache.h"
#include <fstream>
#include <vector>
#include <stack>
#include <functional>
#include <shlobj.h>
#include "DiffContext.h"
#include "CompareStats.h"
#include "DiffItem.h"
#include "DirItem.h"
#include "PathContext.h"
#include "paths.h"

namespace
{
	static const char CACHE_MAGIC[4] = {'W', 'M', 'D', 'C'};
	static const uint32_t CACHE_VERSION = 1;

	// Helper: write a length-prefixed wstring to a binary stream
	void WriteString(std::ofstream& f, const String& s)
	{
		uint32_t len = static_cast<uint32_t>(s.length());
		f.write(reinterpret_cast<const char*>(&len), sizeof(len));
		if (len > 0)
			f.write(reinterpret_cast<const char*>(s.c_str()), len * sizeof(wchar_t));
	}

	// Helper: read a length-prefixed wstring from a binary stream
	bool ReadString(std::ifstream& f, String& s)
	{
		uint32_t len = 0;
		f.read(reinterpret_cast<char*>(&len), sizeof(len));
		if (!f.good())
			return false;
		if (len > 1000000) // sanity check
			return false;
		if (len == 0)
		{
			s.clear();
			return true;
		}
		s.resize(len);
		f.read(reinterpret_cast<char*>(&s[0]), len * sizeof(wchar_t));
		return f.good();
	}

	// Helper: write a DiffFileInfo side to stream
	void WriteSideInfo(std::ofstream& f, const DiffFileInfo& info)
	{
		String path = info.path.get();
		String filename = info.filename.get();
		WriteString(f, path);
		WriteString(f, filename);
		int64_t mtime = info.mtime.raw();
		int64_t ctime = info.ctime.raw();
		int64_t size = static_cast<int64_t>(info.size);
		uint32_t attrs = info.flags.attributes;
		f.write(reinterpret_cast<const char*>(&mtime), sizeof(mtime));
		f.write(reinterpret_cast<const char*>(&ctime), sizeof(ctime));
		f.write(reinterpret_cast<const char*>(&size), sizeof(size));
		f.write(reinterpret_cast<const char*>(&attrs), sizeof(attrs));
	}

	// Helper: read a DiffFileInfo side from stream
	bool ReadSideInfo(std::ifstream& f, DiffFileInfo& info)
	{
		String path, filename;
		if (!ReadString(f, path)) return false;
		if (!ReadString(f, filename)) return false;
		info.path = path;
		info.filename = filename;
		int64_t mtime, ctime, size;
		uint32_t attrs;
		f.read(reinterpret_cast<char*>(&mtime), sizeof(mtime));
		f.read(reinterpret_cast<char*>(&ctime), sizeof(ctime));
		f.read(reinterpret_cast<char*>(&size), sizeof(size));
		f.read(reinterpret_cast<char*>(&attrs), sizeof(attrs));
		if (!f.good()) return false;
		info.mtime = Poco::Timestamp(mtime);
		info.ctime = Poco::Timestamp(ctime);
		info.size = static_cast<Poco::File::FileSize>(size);
		info.flags.attributes = attrs;
		return true;
	}

	// Get the directory mtime as an int64
	int64_t GetDirectoryMtime(const String& dirPath)
	{
		WIN32_FILE_ATTRIBUTE_DATA fad = {};
		if (::GetFileAttributesExW(dirPath.c_str(), GetFileExInfoStandard, &fad))
		{
			LARGE_INTEGER li;
			li.LowPart = fad.ftLastWriteTime.dwLowDateTime;
			li.HighPart = fad.ftLastWriteTime.dwHighDateTime;
			return li.QuadPart;
		}
		return 0;
	}

	// Count all DIFFITEM nodes in the tree (for serialization header)
	uint32_t CountItems(const CDiffContext& ctx, const DIFFITEM* parent)
	{
		uint32_t count = 0;
		DIFFITEM* pos = const_cast<CDiffContext&>(ctx).GetFirstChildDiffPosition(parent);
		while (pos != nullptr)
		{
			count++;
			DIFFITEM* curpos = pos;
			const DIFFITEM& di = const_cast<CDiffContext&>(ctx).GetNextSiblingDiffRefPosition(pos);
			if (di.HasChildren())
				count += CountItems(ctx, curpos);
			pos = curpos;
			const_cast<CDiffContext&>(ctx).GetNextSiblingDiffRefPosition(pos);
		}
		return count;
	}

	// Serialize DIFFITEM tree depth-first pre-order
	void WriteItems(std::ofstream& f, const CDiffContext& ctx, const DIFFITEM* parent,
		uint16_t depth, int nDirs)
	{
		DIFFITEM* pos = const_cast<CDiffContext&>(ctx).GetFirstChildDiffPosition(parent);
		while (pos != nullptr)
		{
			DIFFITEM* curpos = pos;
			const DIFFITEM& di = const_cast<CDiffContext&>(ctx).GetNextSiblingDiffRefPosition(pos);

			f.write(reinterpret_cast<const char*>(&depth), sizeof(depth));
			uint32_t diffcode = di.diffcode.diffcode;
			f.write(reinterpret_cast<const char*>(&diffcode), sizeof(diffcode));
			uint32_t customFlags = di.customFlags;
			f.write(reinterpret_cast<const char*>(&customFlags), sizeof(customFlags));
			int32_t nsdiffs = di.nsdiffs;
			int32_t nidiffs = di.nidiffs;
			f.write(reinterpret_cast<const char*>(&nsdiffs), sizeof(nsdiffs));
			f.write(reinterpret_cast<const char*>(&nidiffs), sizeof(nidiffs));

			for (int side = 0; side < nDirs; side++)
				WriteSideInfo(f, di.diffFileInfo[side]);

			if (di.HasChildren())
				WriteItems(f, ctx, curpos, depth + 1, nDirs);

			pos = curpos;
			const_cast<CDiffContext&>(ctx).GetNextSiblingDiffRefPosition(pos);
		}
	}
}

namespace DirScanCache
{

String GetCachePath(const PathContext& paths, int nCompMethod, bool bRecursive)
{
	// Build a deterministic cache key from the comparison parameters
	String key;
	for (int i = 0; i < paths.GetSize(); i++)
	{
		if (i > 0) key += _T("|");
		key += paths.GetPath(i, true);
	}
	key += strutils::format(_T("|%d|%d"), nCompMethod, bRecursive ? 1 : 0);

	// Simple hash of the key string
	uint32_t hash = 0;
	for (auto ch : key)
	{
		hash = hash * 31 + static_cast<uint32_t>(ch);
	}

	// Build path: %TEMP%\WinMerge\DirScanCache\<hash>.bin
	wchar_t tempPath[MAX_PATH] = {};
	::GetTempPathW(MAX_PATH, tempPath);
	String cachePath = tempPath;
	cachePath += _T("WinMerge\\DirScanCache\\");
	cachePath += strutils::format(_T("%08X.bin"), hash);
	return cachePath;
}

bool IsCacheValid(const PathContext& paths, int nCompMethod, bool bRecursive)
{
	String cachePath = GetCachePath(paths, nCompMethod, bRecursive);
	std::ifstream f(cachePath, std::ios::binary);
	if (!f.is_open())
		return false;

	// Read and verify magic
	char magic[4];
	f.read(magic, 4);
	if (!f.good() || memcmp(magic, CACHE_MAGIC, 4) != 0)
		return false;

	// Read and verify version
	uint32_t version;
	f.read(reinterpret_cast<char*>(&version), sizeof(version));
	if (!f.good() || version != CACHE_VERSION)
		return false;

	// Read paths and verify they match
	String leftPath, rightPath;
	if (!ReadString(f, leftPath) || !ReadString(f, rightPath))
		return false;
	if (leftPath != paths.GetPath(0, true) || rightPath != paths.GetPath(1, true))
		return false;

	// Read compare method and recursive flag
	int32_t cmpMethod;
	uint8_t recursive;
	f.read(reinterpret_cast<char*>(&cmpMethod), sizeof(cmpMethod));
	f.read(reinterpret_cast<char*>(&recursive), sizeof(recursive));
	if (!f.good() || cmpMethod != nCompMethod || recursive != (bRecursive ? 1 : 0))
		return false;

	// Read and verify root directory mtimes
	int64_t leftMtime, rightMtime;
	f.read(reinterpret_cast<char*>(&leftMtime), sizeof(leftMtime));
	f.read(reinterpret_cast<char*>(&rightMtime), sizeof(rightMtime));
	if (!f.good())
		return false;

	int64_t currentLeftMtime = GetDirectoryMtime(paths.GetPath(0, true));
	int64_t currentRightMtime = GetDirectoryMtime(paths.GetPath(1, true));
	return (leftMtime == currentLeftMtime && rightMtime == currentRightMtime);
}

bool SaveCache(const CDiffContext& ctx, const PathContext& paths,
	int nCompMethod, bool bRecursive)
{
	// Only support 2-dir comparison for now
	if (paths.GetSize() != 2)
		return false;

	String cachePath = GetCachePath(paths, nCompMethod, bRecursive);

	// Ensure directory exists
	String cacheDir = paths::GetParentPath(cachePath);
	::CreateDirectoryW(paths::GetParentPath(cacheDir).c_str(), nullptr);
	::CreateDirectoryW(cacheDir.c_str(), nullptr);

	std::ofstream f(cachePath, std::ios::binary | std::ios::trunc);
	if (!f.is_open())
		return false;

	// Write header
	f.write(CACHE_MAGIC, 4);
	f.write(reinterpret_cast<const char*>(&CACHE_VERSION), sizeof(CACHE_VERSION));

	String leftPath = paths.GetPath(0, true);
	String rightPath = paths.GetPath(1, true);
	WriteString(f, leftPath);
	WriteString(f, rightPath);

	int32_t cmpMethod = nCompMethod;
	uint8_t recursive = bRecursive ? 1 : 0;
	f.write(reinterpret_cast<const char*>(&cmpMethod), sizeof(cmpMethod));
	f.write(reinterpret_cast<const char*>(&recursive), sizeof(recursive));

	int64_t leftMtime = GetDirectoryMtime(leftPath);
	int64_t rightMtime = GetDirectoryMtime(rightPath);
	f.write(reinterpret_cast<const char*>(&leftMtime), sizeof(leftMtime));
	f.write(reinterpret_cast<const char*>(&rightMtime), sizeof(rightMtime));

	uint32_t itemCount = CountItems(ctx, nullptr);
	f.write(reinterpret_cast<const char*>(&itemCount), sizeof(itemCount));

	int nDirs = ctx.GetCompareDirs();
	WriteItems(f, ctx, nullptr, 0, nDirs);

	return f.good();
}

bool LoadCache(CDiffContext& ctx, const PathContext& paths,
	int nCompMethod, bool bRecursive)
{
	// Only support 2-dir comparison for now
	if (paths.GetSize() != 2)
		return false;

	String cachePath = GetCachePath(paths, nCompMethod, bRecursive);
	std::ifstream f(cachePath, std::ios::binary);
	if (!f.is_open())
		return false;

	// Read and verify header
	char magic[4];
	f.read(magic, 4);
	if (!f.good() || memcmp(magic, CACHE_MAGIC, 4) != 0)
		return false;

	uint32_t version;
	f.read(reinterpret_cast<char*>(&version), sizeof(version));
	if (!f.good() || version != CACHE_VERSION)
		return false;

	String leftPath, rightPath;
	if (!ReadString(f, leftPath) || !ReadString(f, rightPath))
		return false;
	if (leftPath != paths.GetPath(0, true) || rightPath != paths.GetPath(1, true))
		return false;

	int32_t cmpMethod;
	uint8_t recursive;
	f.read(reinterpret_cast<char*>(&cmpMethod), sizeof(cmpMethod));
	f.read(reinterpret_cast<char*>(&recursive), sizeof(recursive));
	if (!f.good() || cmpMethod != nCompMethod || recursive != (bRecursive ? 1 : 0))
		return false;

	int64_t leftMtime, rightMtime;
	f.read(reinterpret_cast<char*>(&leftMtime), sizeof(leftMtime));
	f.read(reinterpret_cast<char*>(&rightMtime), sizeof(rightMtime));
	if (!f.good())
		return false;

	// Validate root directory mtimes
	int64_t currentLeftMtime = GetDirectoryMtime(paths.GetPath(0, true));
	int64_t currentRightMtime = GetDirectoryMtime(paths.GetPath(1, true));
	if (leftMtime != currentLeftMtime || rightMtime != currentRightMtime)
		return false;

	uint32_t itemCount;
	f.read(reinterpret_cast<char*>(&itemCount), sizeof(itemCount));
	if (!f.good() || itemCount > 10000000) // sanity limit
		return false;

	int nDirs = ctx.GetCompareDirs();

	// Read items and rebuild tree. We use a stack to track parents by depth.
	// depth 0 = root-level items (parent = nullptr for AddNewDiff)
	std::stack<std::pair<uint16_t, DIFFITEM*>> parentStack;

	for (uint32_t i = 0; i < itemCount; i++)
	{
		uint16_t depth;
		f.read(reinterpret_cast<char*>(&depth), sizeof(depth));
		if (!f.good())
			return false;

		uint32_t diffcode;
		f.read(reinterpret_cast<char*>(&diffcode), sizeof(diffcode));
		uint32_t customFlags;
		f.read(reinterpret_cast<char*>(&customFlags), sizeof(customFlags));
		int32_t nsdiffs, nidiffs;
		f.read(reinterpret_cast<char*>(&nsdiffs), sizeof(nsdiffs));
		f.read(reinterpret_cast<char*>(&nidiffs), sizeof(nidiffs));
		if (!f.good())
			return false;

		// Determine parent for this depth
		while (!parentStack.empty() && parentStack.top().first >= depth)
			parentStack.pop();

		DIFFITEM* parent = parentStack.empty() ? nullptr : parentStack.top().second;
		DIFFITEM* di = ctx.AddNewDiff(parent);

		di->diffcode.diffcode = diffcode;
		di->customFlags = customFlags;
		di->nsdiffs = nsdiffs;
		di->nidiffs = nidiffs;

		for (int side = 0; side < nDirs; side++)
		{
			if (!ReadSideInfo(f, di->diffFileInfo[side]))
				return false;
		}
		// For 2-dir comparison, fill side 2 same as side 0 (placeholder)
		if (nDirs == 2)
		{
			di->diffFileInfo[2].path = di->diffFileInfo[0].path;
			di->diffFileInfo[2].filename = di->diffFileInfo[0].filename;
		}

		// Push this item as potential parent for deeper items
		parentStack.push({depth, di});

		ctx.m_pCompareStats->IncreaseTotalItems();
		ctx.m_pCompareStats->AddItem(diffcode);
	}

	return f.good();
}

} // namespace DirScanCache
