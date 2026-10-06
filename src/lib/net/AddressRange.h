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

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inputleap {

// Scanning is sequential and each dead address costs a connect timeout, so a
// range much larger than a home or office subnet is almost certainly a typo.
constexpr std::size_t kMaxAddressRangeSize = 1024;

/*!
Expands an IPv4 range into the addresses it contains, in ascending order.

Two forms are accepted: a full range such as "192.168.1.10-192.168.1.50",
and a short form that gives only the last octet of the end, such as
"192.168.1.10-50".

Anything that does not start with an IPv4 address followed by '-' is
returned unchanged as the only element. Hostnames may legally contain
hyphens ("my-pc"), and IPv6 addresses are not supported as ranges.

Throws std::invalid_argument if \p host starts like a range but the end is
malformed, the end is before the start, or the range holds more than
kMaxAddressRangeSize addresses.
*/
std::vector<std::string> expand_address_range(const std::string& host);

} // namespace inputleap
