/*  InputLeap -- mouse and keyboard sharing utility
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

#include "../src/MonitorLayout.h"
#include <gtest/gtest.h>

#include <cmath>

using Edge = MonitorLayout::Edge;

namespace {

::testing::AssertionResult hasInterval(const MonitorLayout& layout, int index, Edge edge,
                                       double start, double end)
{
    double s = -1;
    double e = -1;
    if (!layout.edgeInterval(index, edge, s, e))
        return ::testing::AssertionFailure() << "monitor " << index << " does not reach that edge";
    if (std::abs(s - start) > 1e-9 || std::abs(e - end) > 1e-9)
        return ::testing::AssertionFailure() << "got (" << s << ", " << e << ")";
    return ::testing::AssertionSuccess();
}

bool reaches(const MonitorLayout& layout, int index, Edge edge)
{
    double s, e;
    return layout.edgeInterval(index, edge, s, e);
}

} // namespace

TEST(MonitorLayoutTests, a_single_monitor_keeps_the_server_as_one_cell)
{
    EXPECT_FALSE(MonitorLayout::fromRects({QRect(0, 0, 1920, 1080)}).isSplit());
    EXPECT_FALSE(MonitorLayout::fromRects({}).isSplit());
}

TEST(MonitorLayoutTests, side_by_side_monitors_share_a_row_and_split_the_top_edge)
{
    auto layout = MonitorLayout::fromRects({QRect(0, 0, 1920, 1080), QRect(1920, 0, 1920, 1080)});
    ASSERT_TRUE(layout.isSplit());
    EXPECT_EQ(QPoint(0, 0), layout.cellOffset(0));
    EXPECT_EQ(QPoint(1, 0), layout.cellOffset(1));
    EXPECT_TRUE(hasInterval(layout, 0, Edge::Top, 0.0, 0.5));
    EXPECT_TRUE(hasInterval(layout, 1, Edge::Top, 0.5, 1.0));
    EXPECT_TRUE(hasInterval(layout, 1, Edge::Bottom, 0.5, 1.0));
}

TEST(MonitorLayoutTests, only_the_outer_monitor_reaches_a_side_edge)
{
    auto layout = MonitorLayout::fromRects({QRect(0, 0, 1920, 1080), QRect(1920, 0, 1920, 1080)});
    EXPECT_TRUE(hasInterval(layout, 0, Edge::Left, 0.0, 1.0));
    EXPECT_FALSE(reaches(layout, 0, Edge::Right));
    EXPECT_FALSE(reaches(layout, 1, Edge::Left));
    EXPECT_TRUE(hasInterval(layout, 1, Edge::Right, 0.0, 1.0));
}

TEST(MonitorLayoutTests, stacked_monitors_share_a_column)
{
    auto layout = MonitorLayout::fromRects({QRect(0, 1080, 1920, 1080), QRect(0, 0, 1920, 1080)});
    ASSERT_TRUE(layout.isSplit());
    // numbered top to bottom whatever order Windows reports them in
    EXPECT_EQ(QPoint(0, 1), layout.cellOffset(1));
    EXPECT_TRUE(hasInterval(layout, 0, Edge::Left, 0.0, 0.5));
    EXPECT_TRUE(hasInterval(layout, 1, Edge::Left, 0.5, 1.0));
}

TEST(MonitorLayoutTests, unequal_widths_split_the_edge_in_proportion)
{
    auto layout = MonitorLayout::fromRects({QRect(0, 0, 2560, 1440), QRect(2560, 0, 1920, 1440)});
    EXPECT_TRUE(hasInterval(layout, 0, Edge::Top, 0.0, 2560.0 / 4480));
    EXPECT_TRUE(hasInterval(layout, 1, Edge::Top, 2560.0 / 4480, 1.0));
}

TEST(MonitorLayoutTests, a_monitor_left_of_the_primary_is_numbered_first)
{
    auto layout = MonitorLayout::fromRects({QRect(0, 0, 1920, 1080), QRect(-1920, 0, 1920, 1080)});
    EXPECT_EQ(QPoint(1, 0), layout.cellOffset(1));
    EXPECT_TRUE(hasInterval(layout, 0, Edge::Top, 0.0, 0.5));
}

TEST(MonitorLayoutTests, a_slightly_lower_monitor_still_sits_beside_its_neighbour)
{
    auto layout = MonitorLayout::fromRects({QRect(0, 0, 1920, 1080), QRect(1920, 100, 1920, 1080)});
    ASSERT_TRUE(layout.isSplit());
    EXPECT_EQ(QPoint(1, 0), layout.cellOffset(1));
    // its top is below the desktop's, so the cursor can never leave upwards
    // from it and no link should be written there
    EXPECT_FALSE(reaches(layout, 1, Edge::Top));
    EXPECT_TRUE(hasInterval(layout, 1, Edge::Bottom, 0.5, 1.0));
}

TEST(MonitorLayoutTests, an_l_shaped_layout_can_place_a_monitor_left_of_the_first)
{
    auto layout = MonitorLayout::fromRects({QRect(1920, 0, 1920, 1080), QRect(0, 1080, 1920, 1080)});
    ASSERT_TRUE(layout.isSplit());
    EXPECT_EQ(QPoint(-1, 1), layout.cellOffset(1));
}

TEST(MonitorLayoutTests, mirrored_monitors_are_not_split)
{
    EXPECT_FALSE(MonitorLayout::fromRects({QRect(0, 0, 1920, 1080), QRect(0, 0, 1920, 1080)})
                         .isSplit());
}
