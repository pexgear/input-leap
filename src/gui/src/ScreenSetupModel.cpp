/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2012-2016 Symless Ltd.
 * Copyright (C) 2008 Volker Lanz (vl@fidra.de)
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 *
 * This package is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScreenSetupModel.h"
#include "Screen.h"
#include "ServerConfig.h"
#include "MonitorLayout.h"

#include <QtCore>
#include <QtGui>

const QString ScreenSetupModel::m_MimeType = "application/x-input-leap-screen";

ScreenSetupModel::ScreenSetupModel(std::vector<Screen>& screens, int numColumns, int numRows) :
    QAbstractTableModel(nullptr),
    m_Screens(screens),
    m_NumColumns(numColumns),
    m_NumRows(numRows)
{
    if (static_cast<std::size_t>(m_NumColumns * m_NumRows) > screens.size())
        qFatal("Not enough elements (%zu) in screens QList for %d columns and %d rows", screens.size(), m_NumColumns, m_NumRows);
}

QVariant ScreenSetupModel::data(const QModelIndex& index, int role) const
{
    if (index.isValid() && index.row() < m_NumRows && index.column() < m_NumColumns)
    {
        switch(role)
        {
            case Qt::DecorationRole:
                if (screen(index).isNull())
                    break;
                return QIcon(*screen(index).pixmap());

            case Qt::ToolTipRole:
                if (screen(index).isNull())
                    break;
                if (screen(index).isMonitorCell())
                    return QString(tr(
                            "<center>Screen: <b>%1</b>, monitor %2</center>"
                            "<br>Place a computer next to this monitor to link it to this monitor only"
                            "<br>Dragging moves all of this computer's monitors"
                            "<br>Double click to edit settings"))
                            .arg(screen(index).name()).arg(screen(index).monitor() + 1);
                return QString(tr(
                            "<center>Screen: <b>%1</b></center>"
                            "<br>Double click to edit settings"
                            "<br>Drag screen to the trashcan to remove it")).arg(screen(index).name());

            case Qt::DisplayRole:
                if (screen(index).isNull())
                    break;
                if (screen(index).isMonitorCell())
                    return QString("%1 (%2)").arg(screen(index).name()).arg(screen(index).monitor() + 1);
                return screen(index).name();
        default:
            break;
        }
    }

    return QVariant();
}

Qt::ItemFlags ScreenSetupModel::flags(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() >= m_NumRows || index.column() >= m_NumColumns) {
#if QT_VERSION >= QT_VERSION_CHECK(5,15,0)
        return Qt::ItemFlags();
#else
        return nullptr;
#endif
    }

    if (!screen(index).isNull())
        return Qt::ItemIsEnabled | Qt::ItemIsDragEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled;
}

Qt::DropActions ScreenSetupModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

QStringList ScreenSetupModel::mimeTypes() const
{
    return QStringList() << m_MimeType;
}

QMimeData* ScreenSetupModel::mimeData(const QModelIndexList& indexes) const
{
    QMimeData* pMimeData = new QMimeData();
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);

    for (const QModelIndex& index : indexes) {
        if (index.isValid())
            stream << index.column() << index.row() << screen(index);
    }

    pMimeData->setData(m_MimeType, encodedData);

    return pMimeData;
}

bool ScreenSetupModel::dropMimeData(const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (!data->hasFormat(m_MimeType))
        return false;

     if (!parent.isValid() || row != -1 || column != -1)
        return false;

    QByteArray encodedData = data->data(m_MimeType);
    QDataStream stream(&encodedData, QIODevice::ReadOnly);

    int sourceColumn = -1;
    int sourceRow = -1;

    stream >> sourceColumn;
    stream >> sourceRow;

    // don't drop screen onto itself
    if (sourceColumn == parent.column() && sourceRow == parent.row())
        return false;

    Screen droppedScreen;
    stream >> droppedScreen;

    if (droppedScreen.isMonitorCell() && m_ServerMonitors && m_ServerMonitors->isSplit()
            && droppedScreen.monitor() < m_ServerMonitors->count())
    {
        // Move all of the server's monitors together, keeping their
        // arrangement, so that the one being dragged lands where it was
        // dropped. Refuse if any of them would not fit.
        QPoint offset = m_ServerMonitors->cellOffset(droppedScreen.monitor());
        std::vector<Screen> cells = m_Screens;
        ServerConfig::removeServerCells(cells, droppedScreen.name());
        if (!ServerConfig::placeServerBlock(cells, m_NumColumns, m_NumRows,
                                            parent.column() - offset.x(),
                                            parent.row() - offset.y(),
                                            droppedScreen, *m_ServerMonitors))
            return false;

        m_Screens = cells;
        m_BlockMoved = true;
        allDataChanged();
        return true;
    }

    // Swapping another screen into one monitor's cell would tear the server
    // apart, so a monitor cell is never a drop target for anything else.
    if (screen(parent.column(), parent.row()).isMonitorCell())
        return false;

    Screen oldScreen = screen(parent.column(), parent.row());
    if (!oldScreen.isNull() && sourceColumn != -1 && sourceRow != -1)
    {
        // mark the screen so it isn't deleted after the dragndrop succeeded
        // see ScreenSetupView::startDrag()
        oldScreen.setSwapped(true);
        screen(sourceColumn, sourceRow) = oldScreen;
    }

    screen(parent.column(), parent.row()) = droppedScreen;

    return true;
}

void ScreenSetupModel::setServerMonitors(const QString& serverName, const MonitorLayout* layout)
{
    m_ServerName = serverName;
    m_ServerMonitors = layout;
    allDataChanged();
}

bool ScreenSetupModel::takeBlockMoved()
{
    bool moved = m_BlockMoved;
    m_BlockMoved = false;
    return moved;
}

void ScreenSetupModel::removeServer(const QString& name)
{
    ServerConfig::removeServerCells(m_Screens, name);
    allDataChanged();
}

void ScreenSetupModel::updateServerCells(const Screen& edited, const QString& oldName)
{
    for (Screen& cell : m_Screens) {
        if (cell.isNull() || cell.name() != oldName)
            continue;
        int monitor = cell.monitor();
        cell = edited;
        cell.setMonitor(monitor);
    }
    allDataChanged();
}

void ScreenSetupModel::allDataChanged()
{
    emit dataChanged(index(0, 0), index(m_NumRows - 1, m_NumColumns - 1));
}
