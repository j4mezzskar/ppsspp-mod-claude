#pragma once

// User-provided artwork, thumbnails and descriptions for games.
//
// Everything lives in <memstick>/PSP/SYSTEM/CUSTOM_ART/<game key>/ (key = file name + a short hash):
//   icon.png|jpg|jpeg   the thumbnail, replaces the game's own icon
//   hero.png|jpg|jpeg   the large artwork, replaces the game's own background
//   description.txt     free text shown on the home screen and game page
//
// Header-only on purpose, so it needs no build-system changes.

#include <cctype>
#include <cstdio>
#include <functional>
#include <fstream>
#include <mutex>
#include <string>

#include "Common/File/FileUtil.h"
#include "Common/File/Path.h"
#include "Core/System.h"
#include "UI/GameInfoCache.h"

namespace CustomArt {

// Larger files are ignored, so one huge photo can't use up all the texture memory.
constexpr size_t MAX_IMAGE_BYTES = 6 * 1024 * 1024;

// Bumped whenever artwork or a description changes, so views know to reload them.
inline int &Revision() {
	static int revision = 0;
	return revision;
}

// The key comes from the game's file location, so it is the same no matter which parts of the
// game's info have been loaded yet. Moving the game file to another folder starts it fresh.
inline std::string KeyFor(const Path &gamePath) {
	std::string name = gamePath.GetFilename();
	std::string key;
	for (char c : name) {
		key.push_back((isalnum((unsigned char)c) || c == '-' || c == '_') ? c : '_');
	}
	if (key.size() > 48) {
		key.resize(48);
	}
	char hash[16];
	snprintf(hash, sizeof(hash), "_%08x", (unsigned int)std::hash<std::string>()(gamePath.ToString()));
	return key + hash;
}

inline Path DirFor(const std::string &key) {
	return GetSysDirectory(DIRECTORY_SYSTEM) / "CUSTOM_ART" / key;
}

inline bool ReadAll(const Path &path, std::string *out, size_t maxBytes = 0) {
	std::ifstream in(path.ToString(), std::ios::binary | std::ios::ate);
	if (!in) {
		return false;
	}
	std::streamoff size = in.tellg();
	if (size <= 0 || (maxBytes && (size_t)size > maxBytes)) {
		return false;
	}
	out->resize((size_t)size);
	in.seekg(0);
	in.read(&(*out)[0], size);
	return (bool)in;
}

// Looks for base.png, base.jpg, base.jpeg in the game's folder.
inline Path FindImage(const std::string &key, const char *base) {
	static const char *const exts[] = { ".png", ".jpg", ".jpeg" };
	const Path dir = DirFor(key);
	for (const char *ext : exts) {
		Path p = dir / (std::string(base) + ext);
		if (File::Exists(p)) {
			return p;
		}
	}
	return Path();
}

inline void RemoveImages(const std::string &key, const char *base) {
	static const char *const exts[] = { ".png", ".jpg", ".jpeg" };
	const Path dir = DirFor(key);
	for (const char *ext : exts) {
		Path p = dir / (std::string(base) + ext);
		if (File::Exists(p)) {
			File::Delete(p);
		}
	}
}

inline bool HasIcon(const std::string &key) { return !FindImage(key, "icon").empty(); }
inline bool HasHero(const std::string &key) { return !FindImage(key, "hero").empty(); }

inline std::string ReadDescription(const std::string &key) {
	std::string text;
	if (!ReadAll(DirFor(key) / "description.txt", &text, 64 * 1024)) {
		return std::string();
	}
	return text;
}

inline bool SaveDescription(const std::string &key, const std::string &text) {
	++Revision();
	File::CreateFullPath(DirFor(key));
	const Path file = DirFor(key) / "description.txt";
	if (text.empty()) {
		if (File::Exists(file)) {
			File::Delete(file);
		}
		return true;
	}
	std::ofstream out(file.ToString(), std::ios::binary | std::ios::trunc);
	if (!out) {
		return false;
	}
	out << text;
	return true;
}

// Copies a picked image into the game's folder as icon.* or hero.*. The picker can hand back
// a content path on some platforms, so this reads through the generic file API, not just paths.
// Returns an empty string on success, or a short message to show the user.
inline std::string ImportImage(const std::string &key, const char *base, const std::string &pickedPath) {
	std::string data;
	if (!ReadAll(Path(pickedPath), &data, MAX_IMAGE_BYTES * 2)) {
		return "Couldn't read that image";
	}
	if (data.size() > MAX_IMAGE_BYTES) {
		return "That image is too large. Please pick one under 6 MB";
	}
	// Decide the extension from the content, not the file name.
	const char *ext = nullptr;
	if (data.size() > 8 && (unsigned char)data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G') {
		ext = ".png";
	} else if (data.size() > 3 && (unsigned char)data[0] == 0xFF && (unsigned char)data[1] == 0xD8) {
		ext = ".jpg";
	} else {
		return "Please pick a PNG or JPEG image";
	}
	++Revision();
	File::CreateFullPath(DirFor(key));
	RemoveImages(key, base);
	std::ofstream out((DirFor(key) / (std::string(base) + ext)).ToString(), std::ios::binary | std::ios::trunc);
	if (!out) {
		return "Couldn't save the image";
	}
	out.write(data.data(), (std::streamsize)data.size());
	return std::string();
}

inline void ResetAll(const std::string &key) {
	++Revision();
	RemoveImages(key, "icon");
	RemoveImages(key, "hero");
	SaveDescription(key, std::string());
}

// Called by the game info loader once the game's own artwork has been read. Swaps in the
// user's files, if any. Safe to call from the loader thread.
inline void ApplyOverrides(GameInfo *info, GameInfoFlags flags) {
	const std::string key = KeyFor(info->GetFilePath());

	if (flags & GameInfoFlags::ICON) {
		std::string data;
		const Path p = FindImage(key, "icon");
		if (!p.empty() && ReadAll(p, &data, MAX_IMAGE_BYTES)) {
			std::lock_guard<std::mutex> guard(info->lock);
			info->icon.data = std::move(data);
			info->icon.dataLoaded = true;
		}
	}
	if (flags & GameInfoFlags::PIC1) {
		std::string data;
		const Path p = FindImage(key, "hero");
		if (!p.empty() && ReadAll(p, &data, MAX_IMAGE_BYTES)) {
			std::lock_guard<std::mutex> guard(info->lock);
			info->pic1.data = std::move(data);
			info->pic1.dataLoaded = true;
		}
	}
}

}  // namespace CustomArt
