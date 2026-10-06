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
#include <gtest/gtest.h>
#include <stdexcept>

namespace inputleap {

using Hosts = std::vector<std::string>;

TEST(AddressRange, a_single_address_is_returned_unchanged)
{
    EXPECT_EQ(Hosts({"192.168.1.10"}), expand_address_range("192.168.1.10"));
}

TEST(AddressRange, a_hostname_with_a_hyphen_is_not_a_range)
{
    EXPECT_EQ(Hosts({"my-pc"}), expand_address_range("my-pc"));
    EXPECT_EQ(Hosts({"office-pc.local"}), expand_address_range("office-pc.local"));
}

TEST(AddressRange, an_ipv6_address_is_returned_unchanged)
{
    EXPECT_EQ(Hosts({"fe80::1"}), expand_address_range("fe80::1"));
}

TEST(AddressRange, the_short_form_gives_only_the_last_octet_of_the_end)
{
    EXPECT_EQ(Hosts({"192.168.1.10", "192.168.1.11", "192.168.1.12"}),
              expand_address_range("192.168.1.10-12"));
}

TEST(AddressRange, the_full_form_can_cross_an_octet_boundary)
{
    EXPECT_EQ(Hosts({"10.0.0.254", "10.0.0.255", "10.0.1.0", "10.0.1.1"}),
              expand_address_range("10.0.0.254-10.0.1.1"));
}

TEST(AddressRange, a_range_of_one_address_is_allowed)
{
    EXPECT_EQ(Hosts({"10.0.0.5"}), expand_address_range("10.0.0.5-5"));
}

TEST(AddressRange, the_top_of_the_address_space_does_not_wrap)
{
    EXPECT_EQ(Hosts({"255.255.255.254", "255.255.255.255"}),
              expand_address_range("255.255.255.254-255"));
}

TEST(AddressRange, a_backwards_range_is_rejected)
{
    EXPECT_THROW(expand_address_range("192.168.1.50-10"), std::invalid_argument);
}

TEST(AddressRange, a_malformed_end_is_rejected)
{
    EXPECT_THROW(expand_address_range("192.168.1.10-"), std::invalid_argument);
    EXPECT_THROW(expand_address_range("192.168.1.10-256"), std::invalid_argument);
    EXPECT_THROW(expand_address_range("192.168.1.10-1.2"), std::invalid_argument);
    EXPECT_THROW(expand_address_range("192.168.1.10-+20"), std::invalid_argument);
}

TEST(AddressRange, a_range_larger_than_the_limit_is_rejected)
{
    EXPECT_NO_THROW(expand_address_range("10.0.0.0-10.0.3.255"));  // exactly 1024
    EXPECT_THROW(expand_address_range("10.0.0.0-10.0.4.0"), std::invalid_argument);
}

} // namespace inputleap
