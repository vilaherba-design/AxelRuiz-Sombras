// AguaDiag: read-only diagnostic F4SE plugin for Fallout 4 VR 1.2.72.
// Licensed under the GNU General Public License version 3 (see LICENSE at the repository root).
//
// Installs NO hook and writes nothing to the game. A background thread reads the game's water system
// (TESWaterSystem singleton, RVA 0x5932540, verified in the decrypted 1.2.72 exe: see README.md) through
// ReadProcessMemory, so a stale pointer gives a failed read instead of a crash, and logs every change to
// Documents\My Games\Fallout4VR\F4SE\AguaDiag.log. F7 writes a MARK line (press it when the water looks wrong).
//
// No CommonLibF4 and no C++ runtime features beyond the basics: builds with any x64 Windows C++ compiler.
#include <windows.h>
#include <shlobj.h>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace
{
	const std::uintptr_t kWaterSystemPtr = 0x5932540;  // TESWaterSystem* (stored by its constructor at RVA 0x7cb230)
	const std::uintptr_t kCtorStore = 0x7cb230;        // bytes 48 89 0D 09 73 16 05 = mov [rip+0x5167309], rcx
	const unsigned char  kCtorBytes[7] = { 0x48, 0x89, 0x0D, 0x09, 0x73, 0x16, 0x05 };

	std::uintptr_t g_base = 0;
	FILE*          g_log = nullptr;
	LONGLONG       g_written = 0;
	const LONGLONG kMaxLog = 64ll * 1024 * 1024;

	bool Rd(std::uintptr_t a_addr, void* a_out, SIZE_T a_n)  // [poll thread] safe read of our own process
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
		if (!g_log || g_written > kMaxLog) {
			return;
		}
		SYSTEMTIME t;
		GetLocalTime(&t);
		g_written += fprintf(g_log, "%02d:%02d:%02d.%03d ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
		va_list ap;
		va_start(ap, a_fmt);
		g_written += vfprintf(g_log, a_fmt, ap);
		va_end(ap);
		fputc('\n', g_log);
	}

	struct Buf
	{
		char   s[1 << 18];
		size_t n = 0;
		void   Add(const char* a_fmt, ...)
		{
			if (n + 1024 > sizeof(s)) {
				return;
			}
			va_list ap;
			va_start(ap, a_fmt);
			n += vsnprintf(s + n, sizeof(s) - n, a_fmt, ap);
			va_end(ap);
		}
		void Hex(std::uintptr_t a_addr, size_t a_len)
		{
			unsigned char b[0x200];
			if (a_len > sizeof(b) || n + a_len * 2 + 16 > sizeof(s)) {
				return;
			}
			if (!Rd(a_addr, b, a_len)) {
				Add("<unreadable>");
				return;
			}
			static const char* d = "0123456789abcdef";
			for (size_t i = 0; i < a_len; ++i) {
				s[n++] = d[b[i] >> 4];
				s[n++] = d[b[i] & 15];
			}
			s[n] = 0;
		}
	};

	// One text snapshot of the water system. Layouts come from the disassembly notes in README.md.
	void Snapshot(Buf& a_b)
	{
		const auto sys = Get<std::uintptr_t>(g_base + kWaterSystemPtr);
		a_b.Add("SYS %llx ", static_cast<unsigned long long>(sys));
		if (!sys) {
			return;
		}
		a_b.Hex(sys, 0x128);
		a_b.Add("\n");
		struct Arr
		{
			const char* name;
			unsigned    data, count, size;
		};
		static const Arr arrs[] = { { "OBJ", 0x18, 0x28, 0x70 }, { "REFL", 0x30, 0x40, 0x88 }, { "GRP", 0x48, 0x58, 0x48 }, { "DISP", 0x60, 0x70, 0x40 } };
		for (const auto& a : arrs) {
			const auto data = Get<std::uintptr_t>(sys + a.data);
			auto       cnt = Get<std::uint32_t>(sys + a.count);
			a_b.Add("%s count=%u\n", a.name, cnt);
			if (cnt > 64) {
				cnt = 64;
			}
			for (std::uint32_t i = 0; i < cnt; ++i) {
				const auto p = Get<std::uintptr_t>(data + 8ull * i);
				a_b.Add(" %s[%u] %llx ", a.name, i, static_cast<unsigned long long>(p));
				a_b.Hex(p, a.size);
				a_b.Add("\n");
				if (a.data == 0x18) {  // water object: +0x20 shape -> +0x178 water shader property -> +0x58 material
					const auto shape = Get<std::uintptr_t>(p + 0x20);
					const auto prop = Get<std::uintptr_t>(shape + 0x178);
					const auto mat = Get<std::uintptr_t>(prop + 0x58);
					a_b.Add("  shape %llx +0x100: ", static_cast<unsigned long long>(shape));
					a_b.Hex(shape + 0x100, 0x20);
					a_b.Add("\n  prop %llx: ", static_cast<unsigned long long>(prop));
					a_b.Hex(prop, 0xA0);
					a_b.Add("\n  mat %llx: ", static_cast<unsigned long long>(mat));
					a_b.Hex(mat, 0x100);
					a_b.Add("\n");
				}
			}
		}
	}

	// Writes the lines of a_new that differ from the same line of a_old (everything when the line count changed).
	void LogDiff(const Buf& a_old, const Buf& a_new, bool a_all)
	{
		auto lines = [](const Buf& a_b) {
			size_t c = 0;
			for (size_t i = 0; i < a_b.n; ++i) {
				c += a_b.s[i] == '\n';
			}
			return c;
		};
		const bool  all = a_all || lines(a_old) != lines(a_new);
		const char* o = a_old.s;
		const char* p = a_new.s;
		const char* oe = a_old.s + a_old.n;
		const char* pe = a_new.s + a_new.n;
		while (p < pe) {
			const char* pn = static_cast<const char*>(memchr(p, '\n', pe - p));
			pn = pn ? pn : pe;
			const char* on = o < oe ? static_cast<const char*>(memchr(o, '\n', oe - o)) : nullptr;
			on = on ? on : oe;
			if (all || (pn - p) != (on - o) || memcmp(p, o, pn - p) != 0) {
				Log("%.*s", static_cast<int>(pn - p), p);
			}
			p = pn + 1;
			o = on + 1;
		}
	}

	DWORD WINAPI Poll(LPVOID)  // [poll thread]
	{
		static Buf bufs[2];
		int        cur = 0;
		bool       f7 = false;
		unsigned   n = 0;
		for (;;) {
			Sleep(100);
			const bool down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
			const bool mark = down && !f7;
			f7 = down;
			Buf& b = bufs[cur];
			const Buf& old = bufs[cur ^ 1];
			b.n = 0;
			b.s[0] = 0;
			Snapshot(b);
			if (mark) {
				Log("MARK (F7) ------------------------------------------------");
			}
			if (mark || b.n != old.n || memcmp(b.s, old.s, b.n) != 0) {
				Log("SNAP %u%s", ++n, mark ? " full" : "");
				LogDiff(old, b, mark);
				fflush(g_log);
				cur ^= 1;
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
		swprintf(path, MAX_PATH * 2, L"%ls\\My Games\\Fallout4VR\\F4SE\\AguaDiag.log", docs);
		CoTaskMemFree(docs);
		g_log = _wfopen(path, L"w");
	}
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
		a_info->name = "AguaDiag";
		a_info->version = 1;
		return true;
	}

	__declspec(dllexport) bool F4SEPlugin_Load(const void*)  // [loader]
	{
		g_base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		OpenLog();
		unsigned char got[7]{};
		if (!Rd(g_base + kCtorStore, got, 7) || memcmp(got, kCtorBytes, 7) != 0) {
			Log("AguaDiag 0.2: this is not the Fallout4VR.exe 1.2.72 it was made for. Doing nothing.");
			if (g_log) {
				fflush(g_log);
			}
			return true;
		}
		Log("AguaDiag 0.2 loaded, base %llx. Read-only: no hooks. F7 = MARK.", static_cast<unsigned long long>(g_base));
		fflush(g_log);
		CloseHandle(CreateThread(nullptr, 0, Poll, nullptr, 0, nullptr));
		return true;
	}
}
