// AxelRuiz-Sombras: render core (phase 1): catch the D3D11 device, fingerprint every shader, hand the game our
// own copies and count how they are bound. Nothing visible changes.
// Licensed under the GNU General Public License version 3: see LICENSE.
#pragma once

namespace sombras::render
{
	void Install();  // [loader] from F4SEPlugin_Load
}
