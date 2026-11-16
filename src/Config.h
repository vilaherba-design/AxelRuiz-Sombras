// AxelRuiz-Sombras: settings from Data\F4SE\Plugins\AxelRuizSombras.ini.
// Licensed under the GNU General Public License version 3: see LICENSE.
#pragma once

#include <istream>
#include <string>

namespace sombras
{
	struct Config
	{
		bool enabled = true;           // [General] bEnabled: 0 = load and do nothing
		bool identicalCopy = true;     // [Phase1] bIdenticalCopy: hand the game our own copy of every shader
		bool logShaders = true;        // [Phase1] bLogShaders: one log line per created shader (hash, stage, size)
		bool logRenderTargets = true;  // [Phase1] bLogRenderTargets: log every render-target texture created
		bool countBinds = true;        // [Phase1] bCountBinds: count shader binds and log a summary
		int  summarySeconds = 10;      // [Phase1] iSummarySeconds: how often the bind summary is written

		void Parse(std::istream& a_in);
	};

	Config& GetConfig();
	void    LoadConfig();  // [loader]
}
