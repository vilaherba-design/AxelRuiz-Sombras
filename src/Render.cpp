// AxelRuiz-Sombras: render core (phase 1). Addresses are RVAs in Fallout4VR.exe 1.2.72, verified in docs/RE-NOTES.md.
// Licensed under the GNU General Public License version 3: see LICENSE.
//
// Everything goes through COM vtables by slot number with untyped pointers, so this file needs no d3d11.h.
// Every patch reads the slot first and chains to what was there (ReShade, BlurAway VR and OpenComposite may
// have hooked the same slots before us).

#include "Render.h"

#include "Config.h"
#include "Sha1.h"

#include <atomic>
#include <chrono>
#include <mutex>
#include <unordered_set>

namespace sombras::render
{
	namespace
	{
		using HRESULT_t = long;

		// RVAs (docs/RE-NOTES.md, "Verified")
		constexpr std::uintptr_t kImmediateContextRva = 0x6235ab0;  // ID3D11DeviceContext*, stored right after device creation
		constexpr std::uintptr_t kSetShadersRva = 0x1d92c00;        // SetShaders(?, vs, hs, ds, ps): VS/HS/DS/PS on the raw context
		constexpr std::uintptr_t kSetShadersCallers[] = { 0x1d97925, 0x2510649, 0x2814d8a, 0x2845e2f, 0x28aa983, 0x28aaf46 };

		// ID3D11Device vtable slots
		constexpr std::size_t kDevCreateTexture2D = 5;
		constexpr std::size_t kDevCreateVertexShader = 12;
		constexpr std::size_t kDevCreatePixelShader = 15;
		constexpr std::size_t kDevCreateComputeShader = 18;
		// ID3D11DeviceContext vtable slots
		constexpr std::size_t kCtxGetDevice = 3;
		constexpr std::size_t kCtxPSSetShader = 9;
		// IUnknown
		constexpr std::size_t kRelease = 2;

		constexpr unsigned kBindRenderTarget = 0x20;
		constexpr int      kMaxRenderTargetLogs = 400;

		template <class T>
		T Slot(void* a_obj, std::size_t a_idx)
		{
			return reinterpret_cast<T>((*reinterpret_cast<std::uintptr_t**>(a_obj))[a_idx]);
		}

		std::uintptr_t PatchSlot(void* a_obj, std::size_t a_idx, std::uintptr_t a_new)
		{
			auto*      slot = *reinterpret_cast<std::uintptr_t**>(a_obj) + a_idx;
			const auto old = *slot;
			if (old != a_new) {
				REL::safe_write(reinterpret_cast<std::uintptr_t>(slot), a_new);
			}
			return old;
		}

		void ReleaseCom(void* a_obj)
		{
			if (a_obj) {
				Slot<unsigned long (*)(void*)>(a_obj, kRelease)(a_obj);
			}
		}

		// ---- state
		std::mutex                 g_oursLock;
		std::unordered_set<void*>  g_ours;  // shader objects we created and handed to the game
		std::atomic<std::uint64_t> g_created[3]{};  // VS, PS, CS
		std::atomic<std::uint64_t> g_copied{};
		std::atomic<std::uint64_t> g_psBinds{}, g_psBindsOurs{}, g_setShadersCalls{}, g_setShadersPsOurs{};
		std::atomic<int>           g_rtLogs{};
		std::atomic<std::int64_t>  g_nextSummary{};
		bool                       g_deviceHooked = false;
		bool                       g_contextHooked = false;

		bool IsOurs(void* a_shader)
		{
			std::scoped_lock l(g_oursLock);
			return g_ours.contains(a_shader);
		}

		void CheckContext();

		void MaybeSummary()
		{
			const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
			auto       next = g_nextSummary.load(std::memory_order_relaxed);
			if (now < next) {
				return;
			}
			const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::seconds(GetConfig().summarySeconds)).count();
			if (!g_nextSummary.compare_exchange_strong(next, now + period)) {
				return;
			}
			std::size_t ours;
			{
				std::scoped_lock l(g_oursLock);
				ours = g_ours.size();
			}
			CheckContext();
			F4SE::log::info("[summary] created VS {} PS {} CS {}, our copies {} (alive {}); PSSetShader {} ({} ours), SetShaders {} ({} with our PS)",
				g_created[0].load(), g_created[1].load(), g_created[2].load(), g_copied.load(), ours,
				g_psBinds.load(), g_psBindsOurs.load(), g_setShadersCalls.load(), g_setShadersPsOurs.load());
		}

		// ---- device hooks [loader] / [render]: shaders are created on loader and render threads
		using CreateShader_t = HRESULT_t (*)(void*, const void*, std::size_t, void*, void**);
		CreateShader_t g_origCreateVS = nullptr;
		CreateShader_t g_origCreatePS = nullptr;
		CreateShader_t g_origCreateCS = nullptr;

		HRESULT_t CreateShaderCommon(int a_stage, CreateShader_t a_orig, void* a_dev, const void* a_code, std::size_t a_len, void* a_link, void** a_out)
		{
			static constexpr const char* kStage[] = { "VS", "PS", "CS" };
			const auto hr = a_orig(a_dev, a_code, a_len, a_link, a_out);
			if (hr < 0 || !a_out || !*a_out) {
				return hr;
			}
			g_created[a_stage].fetch_add(1, std::memory_order_relaxed);
			const auto& cfg = GetConfig();
			if (cfg.logShaders) {
				F4SE::log::info("[shader] {} {} size {}", kStage[a_stage], ShaderKey(a_code, a_len), a_len);
			}
			if (cfg.identicalCopy) {
				// Phase 1 proof: build a second object from the same bytecode and give the game that one instead.
				void* copy = nullptr;
				if (a_orig(a_dev, a_code, a_len, a_link, &copy) >= 0 && copy) {
					ReleaseCom(*a_out);
					*a_out = copy;
					g_copied.fetch_add(1, std::memory_order_relaxed);
					std::scoped_lock l(g_oursLock);
					g_ours.insert(copy);
				} else {
					F4SE::log::warn("[shader] {} {}: copy failed, the game keeps its own", kStage[a_stage], ShaderKey(a_code, a_len));
				}
			}
			return hr;
		}

		HRESULT_t Hook_CreateVS(void* d, const void* c, std::size_t n, void* l, void** o) { return CreateShaderCommon(0, g_origCreateVS, d, c, n, l, o); }
		HRESULT_t Hook_CreatePS(void* d, const void* c, std::size_t n, void* l, void** o) { return CreateShaderCommon(1, g_origCreatePS, d, c, n, l, o); }
		HRESULT_t Hook_CreateCS(void* d, const void* c, std::size_t n, void* l, void** o) { return CreateShaderCommon(2, g_origCreateCS, d, c, n, l, o); }

		struct Texture2DDesc
		{
			std::uint32_t width, height, mipLevels, arraySize, format, sampleCount, sampleQuality, usage, bindFlags, cpuAccessFlags, miscFlags;
		};
		static_assert(sizeof(Texture2DDesc) == 44);

		using CreateTexture2D_t = HRESULT_t (*)(void*, const Texture2DDesc*, const void*, void**);
		CreateTexture2D_t g_origCreateTexture2D = nullptr;

		HRESULT_t Hook_CreateTexture2D(void* a_dev, const Texture2DDesc* a_desc, const void* a_init, void** a_out)
		{
			const auto hr = g_origCreateTexture2D(a_dev, a_desc, a_init, a_out);
			if (a_desc && (a_desc->bindFlags & kBindRenderTarget) && GetConfig().logRenderTargets && g_rtLogs.fetch_add(1) < kMaxRenderTargetLogs) {
				F4SE::log::info("[rt] {}x{} format {} array {} mips {} samples {} bind {:X} misc {:X} hr {:X}", a_desc->width, a_desc->height, a_desc->format,
					a_desc->arraySize, a_desc->mipLevels, a_desc->sampleCount, a_desc->bindFlags, a_desc->miscFlags, static_cast<std::uint32_t>(hr));
			}
			return hr;
		}

		void HookDevice(void* a_dev)  // [loader]
		{
			if (g_deviceHooked || !a_dev) {
				return;
			}
			g_deviceHooked = true;
			g_origCreateVS = reinterpret_cast<CreateShader_t>(PatchSlot(a_dev, kDevCreateVertexShader, reinterpret_cast<std::uintptr_t>(&Hook_CreateVS)));
			g_origCreatePS = reinterpret_cast<CreateShader_t>(PatchSlot(a_dev, kDevCreatePixelShader, reinterpret_cast<std::uintptr_t>(&Hook_CreatePS)));
			g_origCreateCS = reinterpret_cast<CreateShader_t>(PatchSlot(a_dev, kDevCreateComputeShader, reinterpret_cast<std::uintptr_t>(&Hook_CreateCS)));
			g_origCreateTexture2D = reinterpret_cast<CreateTexture2D_t>(PatchSlot(a_dev, kDevCreateTexture2D, reinterpret_cast<std::uintptr_t>(&Hook_CreateTexture2D)));
			F4SE::log::info("device {:X} hooked: CreateVertexShader/PixelShader/ComputeShader/Texture2D (previous {:X} {:X} {:X} {:X})", reinterpret_cast<std::uintptr_t>(a_dev),
				reinterpret_cast<std::uintptr_t>(g_origCreateVS), reinterpret_cast<std::uintptr_t>(g_origCreatePS),
				reinterpret_cast<std::uintptr_t>(g_origCreateCS), reinterpret_cast<std::uintptr_t>(g_origCreateTexture2D));
		}

		// ---- context hooks [render]
		using PSSetShader_t = void (*)(void*, void*, void* const*, unsigned);
		PSSetShader_t g_origPSSetShader = nullptr;

		void Hook_PSSetShader(void* a_ctx, void* a_ps, void* const* a_inst, unsigned a_n)
		{
			g_psBinds.fetch_add(1, std::memory_order_relaxed);
			if (a_ps && IsOurs(a_ps)) {
				g_psBindsOurs.fetch_add(1, std::memory_order_relaxed);
			}
			MaybeSummary();
			g_origPSSetShader(a_ctx, a_ps, a_inst, a_n);
		}

		void HookContext(void* a_ctx)  // [loader]
		{
			if (g_contextHooked || !a_ctx || !GetConfig().countBinds) {
				return;
			}
			g_contextHooked = true;
			g_origPSSetShader = reinterpret_cast<PSSetShader_t>(PatchSlot(a_ctx, kCtxPSSetShader, reinterpret_cast<std::uintptr_t>(&Hook_PSSetShader)));
			F4SE::log::info("context {:X} hooked: PSSetShader (previous {:X})", reinterpret_cast<std::uintptr_t>(a_ctx), reinterpret_cast<std::uintptr_t>(g_origPSSetShader));
		}

		// ---- the game's SetShaders [render]: game shader objects keep the D3D11 shader at +0x08
		using SetShaders_t = void (*)(void*, void*, void*, void*, void*);
		SetShaders_t g_origSetShaders = nullptr;

		void Hook_SetShaders(void* a_this, void* a_vs, void* a_hs, void* a_ds, void* a_ps)
		{
			g_setShadersCalls.fetch_add(1, std::memory_order_relaxed);
			MaybeSummary();
			if (a_ps) {
				if (auto* d3d = *reinterpret_cast<void**>(static_cast<std::byte*>(a_ps) + 8); d3d && IsOurs(d3d)) {
					g_setShadersPsOurs.fetch_add(1, std::memory_order_relaxed);
				}
			}
			g_origSetShaders(a_this, a_vs, a_hs, a_ds, a_ps);
		}

		void HookSetShaders()  // [loader]
		{
			const auto base = REL::Module::get().base();
			const auto target = base + kSetShadersRva;
			int        n = 0;
			for (const auto rva : kSetShadersCallers) {
				const auto site = base + rva;
				const auto* p = reinterpret_cast<const std::uint8_t*>(site);
				const auto  dst = site + 5 + *reinterpret_cast<const std::int32_t*>(p + 1);
				if (p[0] != 0xE8 || dst != target) {
					F4SE::log::warn("SetShaders call site {:X} does not call {:X} (another mod patched it?): skipped", rva, kSetShadersRva);
					continue;
				}
				const auto orig = F4SE::GetTrampoline().write_call<5>(site, reinterpret_cast<std::uintptr_t>(&Hook_SetShaders));
				if (!g_origSetShaders) {
					g_origSetShaders = reinterpret_cast<SetShaders_t>(orig);
				}
				++n;
			}
			F4SE::log::info("SetShaders: {} of {} call sites hooked", n, std::size(kSetShadersCallers));
		}

		// ---- device creation [loader]
		using CreateDeviceAndSwapChain_t = HRESULT_t (*)(void*, int, void*, unsigned, const void*, unsigned, unsigned, const void*, void**, void**, void*, void**);
		CreateDeviceAndSwapChain_t g_origCreateDevice = nullptr;

		HRESULT_t Hook_CreateDeviceAndSwapChain(void* a_adapter, int a_driver, void* a_soft, unsigned a_flags, const void* a_levels, unsigned a_nLevels,
			unsigned a_sdk, const void* a_scDesc, void** a_sc, void** a_dev, void* a_level, void** a_ctx)
		{
			const auto hr = g_origCreateDevice(a_adapter, a_driver, a_soft, a_flags, a_levels, a_nLevels, a_sdk, a_scDesc, a_sc, a_dev, a_level, a_ctx);
			F4SE::log::info("D3D11CreateDeviceAndSwapChain returned {:X}", static_cast<std::uint32_t>(hr));
			if (hr >= 0) {
				if (a_dev) {
					HookDevice(*a_dev);
				}
				if (a_ctx) {
					HookContext(*a_ctx);
				}
			}
			return hr;
		}
	}

	namespace
	{
		// [render] In 0.1.0 PSSetShader stopped reaching our hook about a minute in (the summary went silent). Log what the game's
		// context pointer and its PSSetShader slot look like now, so we can tell a new context from a re-patched slot.
		// Diagnostic only: re-patching a slot someone else chained to ours could loop.
		void CheckContext()
		{
			static void*          lastCtx = nullptr;
			static std::uintptr_t lastVtbl = 0, lastSlot = 0;
			auto*                 ctx = *reinterpret_cast<void**>(REL::Module::get().base() + kImmediateContextRva);
			if (!ctx) {
				return;
			}
			const auto vtbl = *reinterpret_cast<std::uintptr_t*>(ctx);
			const auto slot = reinterpret_cast<std::uintptr_t*>(vtbl)[kCtxPSSetShader];
			if (ctx != lastCtx || vtbl != lastVtbl || slot != lastSlot) {
				F4SE::log::info("[context] game context {:X} vtable {:X} PSSetShader slot {:X} ({}), our hook {:X}", reinterpret_cast<std::uintptr_t>(ctx), vtbl, slot,
					slot == reinterpret_cast<std::uintptr_t>(&Hook_PSSetShader) ? "ours" : "NOT ours", reinterpret_cast<std::uintptr_t>(&Hook_PSSetShader));
				lastCtx = ctx, lastVtbl = vtbl, lastSlot = slot;
			}
		}
	}

	void Install()
	{
		const auto& cfg = GetConfig();
		F4SE::log::info("config: bEnabled {} bIdenticalCopy {} bLogShaders {} bLogRenderTargets {} bCountBinds {} iSummarySeconds {}", cfg.enabled,
			cfg.identicalCopy, cfg.logShaders, cfg.logRenderTargets, cfg.countBinds, cfg.summarySeconds);
		if (!cfg.enabled) {
			F4SE::log::info("disabled in the INI: nothing hooked");
			return;
		}
		auto* existingCtx = *reinterpret_cast<void**>(REL::Module::get().base() + kImmediateContextRva);
		if (existingCtx) {
			// The device already exists: shader creation already happened, but binds can still be counted.
			F4SE::log::warn("the D3D11 device already exists at plugin load: early shaders were missed");
			void* dev = nullptr;
			Slot<void (*)(void*, void**)>(existingCtx, kCtxGetDevice)(existingCtx, &dev);
			HookDevice(dev);
			ReleaseCom(dev);  // GetDevice added a reference
			HookContext(existingCtx);
		} else {
			g_origCreateDevice = reinterpret_cast<CreateDeviceAndSwapChain_t>(
				REL::PatchIAT(&Hook_CreateDeviceAndSwapChain, "d3d11.dll", "D3D11CreateDeviceAndSwapChain"));
			F4SE::log::info("D3D11CreateDeviceAndSwapChain import {}", g_origCreateDevice ? "patched" : "NOT FOUND: nothing will be hooked");
		}
		if (cfg.countBinds) {
			HookSetShaders();
		}
	}
}
