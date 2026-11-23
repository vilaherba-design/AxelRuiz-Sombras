// LOD Water Fix VR: hides the distant (LOD) water tiles of Fallout 4 VR 1.2.72.
// Licensed under the GNU General Public License version 3 (see LICENSE at the repository root).
//
// The bug: in VR the game keeps drawing its LOD water tiles (4, 8 and 32 cell blocks) on top of the real water of
// the loaded cells. LOD water has no depth, refraction or waves, so it shows as a flat bright sheet with straight
// edges that jumps around as the head turns. Found with tools/water-diag (see its README.md for the addresses).
//
// The fix: a background thread walks the water system's object list (TESWaterSystem, RVA 0x5932540) and sets the
// "hidden" flag of every water shape whose water property has flag 2 (the LOD tiles). No hook, no code patch.
// Memory is read through ReadProcessMemory so a stale pointer gives a failed read instead of a crash.
//
// No CommonLibF4: builds with any x64 Windows C++ compiler (see build.sh).
#include <windows.h>
#include <shlobj.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
	const char*          kVersion = "1.0.1";
	const std::uintptr_t kWaterSystemPtr = 0x5932540;  // TESWaterSystem* (stored by its constructor at RVA 0x7cb230)
	const std::uintptr_t kCtorStore = 0x7cb230;        // 48 89 0D 09 73 16 05 = mov [rip+0x5167309], rcx
	const unsigned char  kCtorBytes[7] = { 0x48, 0x89, 0x0D, 0x09, 0x73, 0x16, 0x05 };

	std::uintptr_t g_base = 0;
	FILE*          g_log = nullptr;
	DWORD          g_intervalMs = 30;
	bool           g_wantLog = false;  // [General] bLog: no log file unless asked for

	bool Rd(std::uintptr_t a_addr, void* a_out, SIZE_T a_n)  // [worker] safe read of our own process
	{
		SIZE_T got = 0;
		return a_addr > 0x10000 && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(a_addr), a_out, a_n, &got) && got == a_n;
	}
	template <class T>
	T Get(std::uintptr_t a_addr)
	{
		T v{};
		Rd(a_addr, &v, sizeof(v));
		return v;
	}

	void Log(const char* a_fmt, ...)
	{
		if (!g_log) {
			return;
		}
		SYSTEMTIME t;
		GetLocalTime(&t);
		fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
		va_list ap;
		va_start(ap, a_fmt);
		vfprintf(g_log, a_fmt, ap);
		va_end(ap);
		fputc('\n', g_log);
		fflush(g_log);
	}

	// [worker] One pass over the water objects. Returns how many LOD tiles it had to hide (were visible).
	unsigned HideLodWater(unsigned& a_lodTiles)
	{
		a_lodTiles = 0;
		const auto sys = Get<std::uintptr_t>(g_base + kWaterSystemPtr);
		if (!sys) {
			return 0;
		}
		const auto data = Get<std::uintptr_t>(sys + 0x18);  // water objects: pointer array, count at +0x28
		auto       count = Get<std::uint32_t>(sys + 0x28);
		if (!data || count > 4096) {
			return 0;
		}
		unsigned hidden = 0;
		for (std::uint32_t i = 0; i < count; ++i) {
			const auto obj = Get<std::uintptr_t>(data + 8ull * i);
			const auto shape = Get<std::uintptr_t>(obj + 0x20);    // NiAVObject of the water plane
			const auto prop = Get<std::uintptr_t>(shape + 0x178);  // its water shader property
			std::uint32_t pflags = 0;
			std::uint64_t sflags = 0;
			if (!shape || !prop || !Rd(prop + 0x70, &pflags, 4) || !Rd(shape + 0x108, &sflags, 8)) {
				continue;
			}
			if (!(pflags & 2)) {  // loaded-cell water: leave it alone
				continue;
			}
			++a_lodTiles;
			if (!(sflags & 1)) {
				InterlockedOr64(reinterpret_cast<volatile LONG64*>(shape + 0x108), 1);  // NiAVObject flags bit 0 = hidden
				++hidden;
			}
		}
		return hidden;
	}

	DWORD WINAPI Worker(LPVOID)  // [worker]
	{
		unsigned long long total = 0;
		unsigned           lastTiles = ~0u;
		DWORD              lastLog = 0;
		for (;;) {
			Sleep(g_intervalMs);
			unsigned   tiles = 0;
			const auto n = HideLodWater(tiles);
			total += n;
			const DWORD now = GetTickCount();
			if (g_log && (n || tiles != lastTiles) && now - lastLog > 10000) {  // at most one line every 10 s
				Log("LOD water tiles: %u (hidden so far: %llu)", tiles, total);
				lastLog = now;
				lastTiles = tiles;
			}
		}
	}

	void OpenLog()
	{
		PWSTR docs = nullptr;
		if (SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs) != S_OK) {
			return;
		}
		wchar_t path[MAX_PATH * 2];
		swprintf(path, MAX_PATH * 2, L"%ls\\My Games\\Fallout4VR\\F4SE\\LODWaterFixVR.log", docs);
		CoTaskMemFree(docs);
		g_log = _wfopen(path, L"w");
	}

	HMODULE g_self = nullptr;

	bool ReadIni()  // [loader] LODWaterFixVR.ini beside the DLL. Returns bEnabled.
	{
		wchar_t path[MAX_PATH * 2]{};
		const DWORD n = GetModuleFileNameW(g_self, path, MAX_PATH * 2 - 1);
		if (n < 4) {
			return true;
		}
		wcscpy(path + n - 3, L"ini");
		g_intervalMs = GetPrivateProfileIntW(L"General", L"iIntervalMs", 30, path);
		if (g_intervalMs < 5) {
			g_intervalMs = 5;
		} else if (g_intervalMs > 10000) {
			g_intervalMs = 10000;
		}
		g_wantLog = GetPrivateProfileIntW(L"General", L"bLog", 0, path) != 0;
		return GetPrivateProfileIntW(L"General", L"bEnabled", 1, path) != 0;
	}
}

BOOL WINAPI DllMain(HINSTANCE a_self, DWORD a_reason, LPVOID)
{
	if (a_reason == DLL_PROCESS_ATTACH) {
		g_self = a_self;
	}
	return TRUE;
}

extern "C"
{
	struct PluginInfo
	{
		std::uint32_t infoVersion;
		const char*   name;
		std::uint32_t version;
	};

	__declspec(dllexport) bool F4SEPlugin_Query(const void*, PluginInfo* a_info)
	{
		a_info->infoVersion = 1;
		a_info->name = "LODWaterFixVR";
		a_info->version = 0x01000100;
		return true;
	}

	__declspec(dllexport) bool F4SEPlugin_Load(const void*)  // [loader]
	{
		g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		const bool enabled = ReadIni();
		if (g_wantLog) {
			OpenLog();
		}
		unsigned char got[7]{};
		if (!Rd(g_base + kCtorStore, got, 7) || memcmp(got, kCtorBytes, 7) != 0) {
			Log("LOD Water Fix VR %s: this is not Fallout4VR.exe 1.2.72. Doing nothing.", kVersion);
			return true;
		}
		if (!enabled) {
			Log("LOD Water Fix VR %s: bEnabled=0. Doing nothing.", kVersion);
			return true;
		}
		Log("LOD Water Fix VR %s loaded. Checking every %lu ms.", kVersion, g_intervalMs);
		CloseHandle(CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr));
		return true;
	}
}
