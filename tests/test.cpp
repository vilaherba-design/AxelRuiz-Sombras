// Off-game tests: SHA-1 fingerprints (must match docs/catalog) and INI parsing.
#include "../src/Config.h"
#include "../src/Sha1.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

int main(int argc, char** argv)
{
	using namespace sombras;
	CHECK(ShaderKey("", 0) == "da39a3ee5e6b4b0d");
	CHECK(ShaderKey("abc", 3) == "a9993e364706816a");
	const std::string m(1000000, 'a');
	CHECK(ShaderKey(m.data(), m.size()) == "34aa973cd4c4daa4");
	const std::string b56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
	CHECK(ShaderKey(b56.data(), b56.size()) == "84983e441c3bd26e");

	{
		Config c;
		std::istringstream in("[General]\nbEnabled = 0 ; off\n[Phase1]\nbIdenticalCopy=0\nBLOGSHADERS=0\niSummarySeconds=99999\nbCountBinds=x\n");
		c.Parse(in);
		CHECK(!c.enabled);
		CHECK(!c.identicalCopy);
		CHECK(!c.logShaders);
		CHECK(c.logRenderTargets);
		CHECK(c.countBinds);
		CHECK(c.summarySeconds == 3600);
	}

	// Optional: <fxp> <catalog.csv>: every DXBC blob of the real file must hash to the catalog's key.
	if (argc == 3) {
		std::ifstream f(argv[1], std::ios::binary);
		const std::vector<char> d((std::istreambuf_iterator<char>(f)), {});
		std::map<long, std::string> cat;
		std::ifstream csv(argv[2]);
		std::string line;
		std::getline(csv, line);
		while (std::getline(csv, line)) {
			std::stringstream ss(line);
			std::string off, size, sha;
			std::getline(ss, off, ',');
			std::getline(ss, size, ',');
			std::getline(ss, sha, ',');
			cat[std::stol(off)] = sha;
		}
		int checked = 0;
		for (auto& [off, sha] : cat) {
			const auto size = *reinterpret_cast<const std::uint32_t*>(d.data() + off + 24);
			if (ShaderKey(d.data() + off, size) != sha) {
				std::printf("FAIL blob at %ld\n", off);
				++g_fail;
			}
			++checked;
		}
		std::printf("checked %d blobs against the catalog\n", checked);
	}
	std::printf(g_fail ? "%d FAILED\n" : "all tests passed\n", g_fail);
	return g_fail ? 1 : 0;
}
