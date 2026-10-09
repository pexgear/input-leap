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

#include "MonitorLayout.h"

#include <QGuiApplication>
#include <QScreen>
#include <QSet>
#include <QStringList>
#include <QtGlobal>

#include <algorithm>

#if defined(Q_OS_WIN)
#define NOMINMAX
#include <Windows.h>
#endif

namespace {

struct Range {
    int start;
    int end;  // exclusive
};

int overlap(Range a, Range b)
{
    return std::min(a.end, b.end) - std::max(a.start, b.start);
}

/*
Groups ranges into bands (rows or columns) and returns each range's band
index, bands numbered in ascending position. A range joins a band when it
overlaps that band's first member by more than half of the shorter of the
two; this keeps slightly offset monitors together while still separating
monitors that are only touching.
*/
QList<int> assignBands(const QList<Range>& ranges)
{
    QList<int> order;
    for (int i = 0; i < ranges.size(); ++i) {
        order.append(i);
    }
    std::sort(order.begin(), order.end(),
              [&](int a, int b) { return ranges[a].start < ranges[b].start; });

    QList<Range> bands;  // the first member of each band, in ascending order
    QList<int> band(ranges.size(), -1);
    for (int i : order) {
        const Range r = ranges[i];
        for (int b = 0; b < bands.size(); ++b) {
            int shorter = std::min(r.end - r.start, bands[b].end - bands[b].start);
            if (overlap(r, bands[b]) * 2 > shorter) {
                band[i] = b;
                break;
            }
        }
        if (band[i] == -1) {
            band[i] = static_cast<int>(bands.size());
            bands.append(r);
        }
    }
    return band;
}

} // namespace

MonitorLayout MonitorLayout::fromRects(const QList<QRect>& monitors)
{
    MonitorLayout layout;
    if (monitors.size() < 2) {
        return layout;
    }

    QList<Range> columns;
    QList<Range> rows;
    QRect bounds;
    for (const QRect& r : monitors) {
        columns.append({r.x(), r.x() + r.width()});
        rows.append({r.y(), r.y() + r.height()});
        bounds = bounds.united(r);
    }
    const QList<int> column = assignBands(columns);
    const QList<int> row = assignBands(rows);

    QList<Monitor> result;
    QSet<QPair<int, int>> used;
    for (int i = 0; i < monitors.size(); ++i) {
        if (used.contains({column[i], row[i]})) {
            // Two monitors in one cell: the arrangement is too irregular for
            // a grid, and guessing would link clients to the wrong monitor.
            return layout;
        }
        used.insert({column[i], row[i]});
        result.append({monitors[i], QPoint(column[i], row[i])});
    }

    std::sort(result.begin(), result.end(), [](const Monitor& a, const Monitor& b) {
        return a.cell.y() != b.cell.y() ? a.cell.y() < b.cell.y() : a.cell.x() < b.cell.x();
    });

    // Offsets are relative to monitor 0, which the grid anchors the server on.
    // The top-left cell may be empty (an L-shaped layout), so this can make
    // an x offset negative; that is fine, the block is still placed whole.
    const QPoint origin = result.front().cell;
    for (Monitor& m : result) {
        m.cell -= origin;
    }

    layout.m_Monitors = result;
    layout.m_Bounds = bounds;
    return layout;
}

QList<QRect> MonitorLayout::currentMonitorRects()
{
    // Lets the multi-monitor grid be exercised on a machine with one monitor:
    // "x,y,w,h;x,y,w,h;..."
    const QByteArray fake = qgetenv("INPUTLEAP_GUI_FAKE_MONITORS");
    if (!fake.isEmpty()) {
        QList<QRect> rects;
        for (const QString& part : QString::fromLatin1(fake).split(';')) {
            const QStringList v = part.split(',');
            if (v.size() == 4) {
                rects.append(QRect(v[0].toInt(), v[1].toInt(), v[2].toInt(), v[3].toInt()));
            }
        }
        return rects;
    }

    QList<QRect> rects;
#if defined(Q_OS_WIN)
    // Qt reports monitor sizes in scaled pixels but positions in physical
    // ones when monitors have different scale factors, which skews the
    // fractions. The server measures its desktop in physical pixels, so ask
    // Windows directly.
    EnumDisplayMonitors(nullptr, nullptr,
        [](HMONITOR, HDC, LPRECT r, LPARAM data) -> BOOL {
            reinterpret_cast<QList<QRect>*>(data)->append(
                QRect(r->left, r->top, r->right - r->left, r->bottom - r->top));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&rects));
#else
    for (const QScreen* screen : QGuiApplication::screens()) {
        rects.append(screen->geometry());
    }
#endif
    return rects;
}

bool MonitorLayout::edgeInterval(int index, Edge edge, double& start, double& end) const
{
    const QRect& r = m_Monitors[index].rect;
    const QRect& b = m_Bounds;

    bool touches = false;
    switch (edge) {
        case Edge::Left:   touches = r.x() == b.x(); break;
        case Edge::Right:  touches = r.x() + r.width() == b.x() + b.width(); break;
        case Edge::Top:    touches = r.y() == b.y(); break;
        case Edge::Bottom: touches = r.y() + r.height() == b.y() + b.height(); break;
    }
    if (!touches) {
        return false;
    }

    if (edge == Edge::Left || edge == Edge::Right) {
        start = double(r.y() - b.y()) / b.height();
        end = double(r.y() + r.height() - b.y()) / b.height();
    } else {
        start = double(r.x() - b.x()) / b.width();
        end = double(r.x() + r.width() - b.x()) / b.width();
    }
    return true;
}

bool MonitorLayout::operator==(const MonitorLayout& other) const
{
    return m_Monitors == other.m_Monitors && m_Bounds == other.m_Bounds;
}
