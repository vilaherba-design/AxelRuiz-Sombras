// AxelRuiz-Sombras: settings from Data\F4SE\Plugins\AxelRuizSombras.ini.
// Licensed under the GNU General Public License version 3: see LICENSE.
#include "Config.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace sombras
{
	namespace
	{
		std::string Trim(std::string s)
		{
			auto sp = [](unsigned char c) { return std::isspace(c) != 0; };
			s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), sp));
			s.erase(std::find_if_not(s.rbegin(), s.rend(), sp).base(), s.end());
			return s;
		}

		std::string Lower(std::string s)
		{
			std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return s;
		}
	}

	void Config::Parse(std::istream& a_in)
	{
		std::string line;
		while (std::getline(a_in, line)) {
			if (const auto c = line.find_first_of(";#"); c != std::string::npos) {
				line.erase(c);
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = Lower(Trim(line.substr(0, eq)));
			const auto val = Trim(line.substr(eq + 1));
			int        n = 0;
			try {
				n = std::stoi(val);
			} catch (...) {
				continue;
			}
			if (key == "benabled") {
				enabled = n != 0;
			} else if (key == "bidenticalcopy") {
				identicalCopy = n != 0;
			} else if (key == "blogshaders") {
				logShaders = n != 0;
			} else if (key == "blogrendertargets") {
				logRenderTargets = n != 0;
			} else if (key == "bcountbinds") {
				countBinds = n != 0;
			} else if (key == "isummaryseconds") {
				summarySeconds = std::clamp(n, 1, 3600);
			}
		}
	}

	Config& GetConfig()
	{
		static Config c;
		return c;
	}

	void LoadConfig()
	{
		std::ifstream f("Data/F4SE/Plugins/AxelRuizSombras.ini");
		if (f) {
			GetConfig().Parse(f);
		}
	}
}
