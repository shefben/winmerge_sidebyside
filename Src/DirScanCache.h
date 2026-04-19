/**
 *  @file DirScanCache.h
 *
 *  @brief Declaration of DirScanCache — binary cache for DIFFITEM trees
 *
 *  Caches the result of directory enumeration (the DIFFITEM tree built by
 *  DirScan_GetItems) to disk so that subsequent opens of the same folder
 *  pair load in <100ms instead of re-enumerating the entire tree.
 */
#pragma once

#include "UnicodeString.h"

class CDiffContext;
class PathContext;

namespace DirScanCache
{
	/**
	 * @brief Save the current DIFFITEM tree to a binary cache file.
	 * @param ctx  The diff context whose tree should be cached.
	 * @param paths Normalized root paths of the comparison.
	 * @param nCompMethod Compare method used (for cache key).
	 * @param bRecursive  Whether subdirectories are included.
	 * @return true on success, false on failure.
	 */
	bool SaveCache(const CDiffContext& ctx, const PathContext& paths,
		int nCompMethod, bool bRecursive);

	/**
	 * @brief Attempt to load a previously saved DIFFITEM tree from cache.
	 * If the cache exists and root directory mtimes haven't changed,
	 * the tree is rebuilt in memory from the cache file, bypassing disk I/O.
	 * @param ctx  The diff context to populate.
	 * @param paths Normalized root paths of the comparison.
	 * @param nCompMethod Compare method used (for cache key).
	 * @param bRecursive  Whether subdirectories are included.
	 * @return true if the cache was loaded successfully, false otherwise.
	 */
	bool LoadCache(CDiffContext& ctx, const PathContext& paths,
		int nCompMethod, bool bRecursive);

	/**
	 * @brief Check whether a valid cache exists for the given parameters.
	 */
	bool IsCacheValid(const PathContext& paths, int nCompMethod, bool bRecursive);

	/**
	 * @brief Get the cache file path for the given comparison parameters.
	 */
	String GetCachePath(const PathContext& paths, int nCompMethod, bool bRecursive);
}
