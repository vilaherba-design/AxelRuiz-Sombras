// AxelRuiz-Sombras: shader framework for Fallout 4 VR 1.2.72.
// Licensed under the GNU General Public License version 3: see LICENSE.
//
// Phase 0 skeleton: the plugin only loads and logs. The render core (D3D11 hooks, shader catalog,
// shader replacement) arrives in phase 1, see docs/PLAN.md.

namespace
{
	void InitLog()
	{
		auto path = F4SE::log::log_directory();
		if (!path) {
			return;
		}
		*path /= "AxelRuizSombras.log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		log->set_level(spdlog::level::debug);
		log->flush_on(spdlog::level::debug);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%Y-%m-%d %T.%e] [%L] %v");
	}
}

extern "C" __declspec(dllexport) constinit auto F4SEPlugin_Version = []() noexcept {
	F4SE::PluginVersionData data{};
	data.PluginName("AxelRuizSombras");
	data.PluginVersion(REL::Version(0, 0, 1, 0));
	data.AuthorName("Axel");
	data.UsesAddressLibrary(true);
	data.IsLayoutDependent(true);
	data.CompatibleVersions({ F4SE::RUNTIME_VR_1_2_72 });
	return data;
}();

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Query(const F4SE::QueryInterface* a_f4se, F4SE::PluginInfo* a_info)
{
	a_info->infoVersion = F4SE::PluginInfo::kVersion;
	a_info->name = "AxelRuizSombras";
	a_info->version = 0x00000100;
	if (a_f4se->IsEditor()) {
		return false;
	}
	// F4SE VR reports 1.10.138 (the flat build VR was forked from) rather than 1.2.72 here, so accept both.
	if (const auto v = a_f4se->RuntimeVersion(); v != F4SE::RUNTIME_VR_1_2_72 && v != REL::Version(1, 10, 138)) {
		return false;
	}
	return true;
}

extern "C" __declspec(dllexport) bool F4SEAPI F4SEPlugin_Load(const F4SE::LoadInterface* a_f4se)
{
	F4SE::Init(a_f4se, false);
	InitLog();  // after Init: the log folder needs F4SE's save folder name (My Games\Fallout4VR\F4SE)
	F4SE::log::info("AxelRuiz-Sombras 0.0.1 (phase 0 skeleton) loaded on runtime {}", REL::Module::get().version().string());
	return true;
}
