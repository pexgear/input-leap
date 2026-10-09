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

#include <QList>
#include <QPoint>
#include <QRect>

/*!
The server computer's monitors, snapped onto the cells of the screen grid.

The server sees its desktop as one rectangle, the bounding box of all its
monitors, and a link can cover just part of one of its edges. This class
turns the real monitor rectangles into grid cells, so a client can be placed
next to one monitor, and into the part of each edge that monitor owns, so
the link written for that cell covers only that monitor.
*/
class MonitorLayout
{
public:
    enum class Edge { Left, Right, Top, Bottom };

    // An unsplit layout: the server is a single cell, exactly as before.
    MonitorLayout() = default;

    /*!
    Builds the layout from monitor rectangles in desktop pixels. Monitors
    whose ranges overlap by more than half of the smaller one share a row
    (or column), so a monitor set a few pixels lower than its neighbour
    still sits beside it. Returns an unsplit layout when there is only one
    monitor, or when two monitors would land in the same cell.
    */
    static MonitorLayout fromRects(const QList<QRect>& monitors);

    //! The monitors of the computer this GUI runs on, in desktop pixels.
    static QList<QRect> currentMonitorRects();

    //! True when the server should be drawn as more than one cell.
    bool isSplit() const { return m_Monitors.size() > 1; }

    int count() const { return static_cast<int>(m_Monitors.size()); }

    /*!
    Grid offset of monitor \p index from monitor 0. Monitors are numbered
    by row, then column, so monitor 0 is the leftmost one in the top row.
    */
    QPoint cellOffset(int index) const { return m_Monitors[index].cell; }

    /*!
    The part of the desktop's \p edge that monitor \p index owns, as
    fractions in [0, 1] of that edge. Returns false when the monitor does
    not reach that edge of the desktop: the cursor can never cross there,
    so no link should be written for it.
    */
    bool edgeInterval(int index, Edge edge, double& start, double& end) const;

    bool operator==(const MonitorLayout& other) const;
    bool operator!=(const MonitorLayout& other) const { return !(*this == other); }

private:
    struct Monitor {
        QRect rect;
        QPoint cell;
        bool operator==(const Monitor& o) const { return rect == o.rect && cell == o.cell; }
    };

    QList<Monitor> m_Monitors;
    QRect m_Bounds;
};
