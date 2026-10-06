/*
    InputLeap -- mouse and keyboard sharing utility
    Copyright (C) InputLeap contributors

    This package is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    found in the file LICENSE that should have accompanied this file.

    This package is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "net/AddressRange.h"

#include <cstdint>
#include <stdexcept>

namespace inputleap {

namespace {

// Strict decimal octet: no sign, no whitespace, at most three digits. strtol
// would accept " 12" and "+12", which would let typos through as ranges.
bool parse_octet(const std::string& s, std::uint32_t& out)
{
    if (s.empty() || s.size() > 3) {
        return false;
    }
    std::uint32_t value = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10 + static_cast<std::uint32_t>(c - '0');
    }
    if (value > 255) {
        return false;
    }
    out = value;
    return true;
}

bool parse_ipv4(const std::string& s, std::uint32_t& out)
{
    std::uint32_t result = 0;
    std::size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        auto dot = s.find('.', start);
        bool last = i == 3;
        if (last != (dot == std::string::npos)) {
            return false;
        }
        std::uint32_t octet = 0;
        if (!parse_octet(s.substr(start, last ? std::string::npos : dot - start), octet)) {
            return false;
        }
        result = (result << 8) | octet;
        start = dot + 1;
    }
    out = result;
    return true;
}

std::string format_ipv4(std::uint32_t addr)
{
    return std::to_string((addr >> 24) & 0xff) + "." +
           std::to_string((addr >> 16) & 0xff) + "." +
           std::to_string((addr >> 8) & 0xff) + "." +
           std::to_string(addr & 0xff);
}

} // namespace

std::vector<std::string> expand_address_range(const std::string& host)
{
    auto dash = host.find('-');
    std::uint32_t first = 0;
    if (dash == std::string::npos || !parse_ipv4(host.substr(0, dash), first)) {
        return { host };
    }

    // From here the text is unmistakably meant as a range; no hostname starts
    // with a dotted quad, so a bad end is a mistake to report rather than a
    // name to hand to the resolver.
    std::string end_text = host.substr(dash + 1);
    std::uint32_t last = 0;
    std::uint32_t last_octet = 0;
    if (parse_ipv4(end_text, last)) {
        // full form
    } else if (parse_octet(end_text, last_octet)) {
        last = (first & 0xffffff00u) | last_octet;
    } else {
        throw std::invalid_argument("invalid end of address range: \"" + end_text + "\"");
    }

    if (last < first) {
        throw std::invalid_argument("address range ends before it starts: \"" + host + "\"");
    }
    if (last - first >= kMaxAddressRangeSize) {
        throw std::invalid_argument("address range is larger than " +
                                    std::to_string(kMaxAddressRangeSize) +
                                    " addresses: \"" + host + "\"");
    }

    std::vector<std::string> result;
    result.reserve(last - first + 1);
    for (std::uint32_t addr = first; ; ++addr) {
        result.push_back(format_ipv4(addr));
        // compare before incrementing so 255.255.255.255 cannot wrap to 0
        if (addr == last) {
            break;
        }
    }
    return result;
}

} // namespace inputleap
