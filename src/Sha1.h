// AxelRuiz-Sombras: small SHA-1, used only to fingerprint DXBC blobs (matches docs/catalog/*.csv, first 16 hex digits).
// Licensed under the GNU General Public License version 3: see LICENSE.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace sombras
{
	inline std::array<std::uint8_t, 20> Sha1(const void* a_data, std::size_t a_len)
	{
		auto rol = [](std::uint32_t v, int s) { return (v << s) | (v >> (32 - s)); };
		std::uint32_t h[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
		const auto*   p = static_cast<const std::uint8_t*>(a_data);
		const std::uint64_t bits = static_cast<std::uint64_t>(a_len) * 8;
		const std::size_t total = ((a_len + 8) / 64 + 1) * 64;
		std::uint8_t block[64];
		for (std::size_t off = 0; off < total; off += 64) {
			for (std::size_t i = 0; i < 64; ++i) {
				const std::size_t k = off + i;
				std::uint8_t b = 0;
				if (k < a_len) {
					b = p[k];
				} else if (k == a_len) {
					b = 0x80;
				} else if (k >= total - 8) {
					b = static_cast<std::uint8_t>(bits >> (8 * (total - 1 - k)));
				}
				block[i] = b;
			}
			std::uint32_t w[80];
			for (int i = 0; i < 16; ++i) {
				w[i] = (std::uint32_t(block[4 * i]) << 24) | (std::uint32_t(block[4 * i + 1]) << 16) | (std::uint32_t(block[4 * i + 2]) << 8) | block[4 * i + 3];
			}
			for (int i = 16; i < 80; ++i) {
				w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
			}
			std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
			for (int i = 0; i < 80; ++i) {
				std::uint32_t f, k;
				if (i < 20) {
					f = (b & c) | (~b & d), k = 0x5A827999;
				} else if (i < 40) {
					f = b ^ c ^ d, k = 0x6ED9EBA1;
				} else if (i < 60) {
					f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
				} else {
					f = b ^ c ^ d, k = 0xCA62C1D6;
				}
				const std::uint32_t t = rol(a, 5) + f + e + k + w[i];
				e = d, d = c, c = rol(b, 30), b = a, a = t;
			}
			h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
		}
		std::array<std::uint8_t, 20> out{};
		for (int i = 0; i < 5; ++i) {
			for (int j = 0; j < 4; ++j) {
				out[4 * i + j] = static_cast<std::uint8_t>(h[i] >> (24 - 8 * j));
			}
		}
		return out;
	}

	// First 16 hex digits of the SHA-1, the key used by docs/catalog.
	inline std::string ShaderKey(const void* a_data, std::size_t a_len)
	{
		static constexpr char kHex[] = "0123456789abcdef";
		const auto d = Sha1(a_data, a_len);
		std::string s(16, '0');
		for (int i = 0; i < 8; ++i) {
			s[2 * i] = kHex[d[i] >> 4];
			s[2 * i + 1] = kHex[d[i] & 0xF];
		}
		return s;
	}
}
