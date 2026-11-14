#pragma once

// Favorite games, saved as one path per line in <memstick>/PSP/SYSTEM/favorites.txt.
// Header-only on purpose, so it needs no build-system changes.

#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

#include "Common/File/FileUtil.h"
#include "Common/File/Path.h"
#include "Core/System.h"

namespace Favorites {

inline std::vector<std::string> &List() {
	static std::vector<std::string> list;
	return list;
}

inline Path FileFor() {
	return GetSysDirectory(DIRECTORY_SYSTEM) / "favorites.txt";
}

inline void Load() {
	static bool loaded = false;
	if (loaded) {
		return;
	}
	loaded = true;
	std::ifstream in(FileFor().ToString());
	std::string line;
	while (std::getline(in, line)) {
		while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
			line.pop_back();
		}
		if (!line.empty()) {
			List().push_back(line);
		}
	}
}

inline void Save() {
	File::CreateFullPath(GetSysDirectory(DIRECTORY_SYSTEM));
	std::ofstream out(FileFor().ToString(), std::ios::trunc);
	for (const std::string &s : List()) {
		out << s << "\n";
	}
}

inline bool Has(const Path &path) {
	Load();
	const std::string s = path.ToString();
	return std::find(List().begin(), List().end(), s) != List().end();
}

// Returns true if the game is now a favorite.
inline bool Toggle(const Path &path) {
	Load();
	const std::string s = path.ToString();
	auto it = std::find(List().begin(), List().end(), s);
	bool now;
	if (it != List().end()) {
		List().erase(it);
		now = false;
	} else {
		List().push_back(s);
		now = true;
	}
	Save();
	return now;
}

inline size_t Count() {
	Load();
	return List().size();
}

// Favorites that still exist on disk.
inline std::vector<Path> Paths() {
	Load();
	std::vector<Path> out;
	for (const std::string &s : List()) {
		Path p(s);
		if (File::Exists(p)) {
			out.push_back(p);
		}
	}
	return out;
}

}  // namespace Favorites
