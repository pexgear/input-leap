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

#include "ServerConfig.h"
#include "Hotkey.h"
#include "MainWindow.h"
#include "AddClientDialog.h"

#include <QtCore>
#include <QMessageBox>
#include <QAbstractButton>
#include <QPushButton>

static const struct
{
     int x;
     int y;
     const char* name;
} neighbourDirs[] =
{
    {  1,  0, "right" },
    { -1,  0, "left" },
    {  0, -1, "up" },
    {  0,  1, "down" },

};

const int serverDefaultIndex = 7;

ServerConfig::ServerConfig(QSettings* settings, int numColumns, int numRows ,
                QString serverName, MainWindow* mainWindow) :
    m_pSettings(settings),
    m_Screens(),
    m_NumColumns(numColumns),
    m_NumRows(numRows),
    m_ServerName(serverName),
    m_IgnoreAutoConfigClient(false),
    m_EnableDragAndDrop(false),
    m_ClipboardSharing(true),
    m_ClipboardSharingSize(defaultClipboardSharingSize()),
    m_pMainWindow(mainWindow)
{
    Q_ASSERT(m_pSettings);

    loadSettings();
}

ServerConfig::~ServerConfig()
{
    saveSettings();
}

bool ServerConfig::save(const QString& fileName) const
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    save(file);
    file.close();

    return true;
}

void ServerConfig::save(QFile& file) const
{
    QTextStream outStream(&file);
    outStream << *this;
}

void ServerConfig::init()
{
    switchCorners().clear();
    screens().clear();

    // m_NumSwitchCorners is used as a fixed size array. See Screen::init()
    for (int i = 0; i < static_cast<int>(SwitchCorner::Count); i++) {
        switchCorners() << false;
    }

    // There must always be screen objects for each cell in the screens QList. Unused screens
    // are identified by having an empty name.
    for (int i = 0; i < numColumns() * numRows(); i++)
        addScreen(Screen());
}

void ServerConfig::saveSettings()
{
    settings().beginGroup("internalConfig");
    settings().remove("");

    settings().setValue("numColumns", numColumns());
    settings().setValue("numRows", numRows());

    settings().setValue("hasHeartbeat", hasHeartbeat());
    settings().setValue("heartbeat", heartbeat());
    settings().setValue("relativeMouseMoves", relativeMouseMoves());
    settings().setValue("screenSaverSync", screenSaverSync());
    settings().setValue("win32KeepForeground", win32KeepForeground());
    settings().setValue("hasSwitchDelay", hasSwitchDelay());
    settings().setValue("switchDelay", switchDelay());
    settings().setValue("hasSwitchDoubleTap", hasSwitchDoubleTap());
    settings().setValue("switchDoubleTap", switchDoubleTap());
    settings().setValue("switchCornerSize", switchCornerSize());
    settings().setValue("ignoreAutoConfigClient", ignoreAutoConfigClient());
    settings().setValue("enableDragAndDrop", enableDragAndDrop());
    settings().setValue("clipboardSharing", clipboardSharing());
    settings().setValue("clipboardSharingSize", (int)clipboardSharingSize());

    writeSettings<bool>(settings(), switchCorners(), "switchCorner");

    settings().beginWriteArray("screens");
    for (std::size_t i = 0; i < screens().size(); i++)
    {
        settings().setArrayIndex(static_cast<int>(i));
        screens()[i].saveSettings(settings());
    }
    settings().endArray();

    settings().beginWriteArray("hotkeys");
    for (std::size_t i = 0; i < hotkeys().size(); i++)
    {
        settings().setArrayIndex(static_cast<int>(i));
        hotkeys()[i].saveSettings(settings());
    }
    settings().endArray();

    settings().endGroup();
}

void ServerConfig::loadSettings()
{
    settings().beginGroup("internalConfig");

    setNumColumns(settings().value("numColumns", 5).toInt());
    setNumRows(settings().value("numRows", 3).toInt());

    // we need to know the number of columns and rows before we can set up ourselves
    init();

    haveHeartbeat(settings().value("hasHeartbeat", false).toBool());
    setHeartbeat(settings().value("heartbeat", 5000).toInt());
    setRelativeMouseMoves(settings().value("relativeMouseMoves", false).toBool());
    setScreenSaverSync(settings().value("screenSaverSync", true).toBool());
    setWin32KeepForeground(settings().value("win32KeepForeground", false).toBool());
    haveSwitchDelay(settings().value("hasSwitchDelay", false).toBool());
    setSwitchDelay(settings().value("switchDelay", 250).toInt());
    haveSwitchDoubleTap(settings().value("hasSwitchDoubleTap", false).toBool());
    setSwitchDoubleTap(settings().value("switchDoubleTap", 250).toInt());
    setSwitchCornerSize(settings().value("switchCornerSize").toInt());
    setIgnoreAutoConfigClient(settings().value("ignoreAutoConfigClient").toBool());
    setEnableDragAndDrop(settings().value("enableDragAndDrop", true).toBool());
    setClipboardSharing(settings().value("clipboardSharing", true).toBool());
    setClipboardSharingSize(settings().value("clipboardSharingSize",
        (int) ServerConfig::defaultClipboardSharingSize()).toULongLong());

    readSettings<bool>(settings(), switchCorners(), "switchCorner", false,
                       static_cast<int>(SwitchCorner::Count));

    std::size_t numScreens = settings().beginReadArray("screens");
    Q_ASSERT(numScreens <= screens().size());
    for (std::size_t i = 0; i < numScreens; i++)
    {
        settings().setArrayIndex(static_cast<int>(i));
        screens()[i].loadSettings(settings());
    }
    settings().endArray();

    int numHotkeys = settings().beginReadArray("hotkeys");
    for (int i = 0; i < numHotkeys; i++)
    {
        settings().setArrayIndex(i);
        Hotkey h;
        h.loadSettings(settings());
        hotkeys().push_back(h);
    }
    settings().endArray();

    settings().endGroup();

    // monitors may have been added, removed or rearranged since last saved
    refreshServerMonitors(m_ServerName);
}

int ServerConfig::adjacentScreenIndex(int idx, int deltaColumn, int deltaRow) const
{
    if (screens()[idx].isNull())
        return -1;

    // if we're at the left or right end of the table, don't find results going further left or right
    if ((deltaColumn > 0 && (idx+1) % numColumns() == 0)
            || (deltaColumn < 0 && idx % numColumns() == 0))
        return -1;

    int arrayPos = idx + deltaColumn + deltaRow * numColumns();

    if (arrayPos >= static_cast<int>(screens().size()) || arrayPos < 0)
        return -1;

    return arrayPos;
}

void ServerConfig::removeServerCells(std::vector<Screen>& cells, const QString& name)
{
    for (Screen& cell : cells) {
        if (!cell.isNull() && cell.name() == name)
            cell = Screen();
    }
}

bool ServerConfig::placeServerBlock(std::vector<Screen>& cells, int columns, int rows,
                                    int anchorColumn, int anchorRow, const Screen& server,
                                    const MonitorLayout& layout)
{
    const int count = layout.isSplit() ? layout.count() : 1;

    // check every target first so a block that does not fit changes nothing
    for (int k = 0; k < count; k++) {
        QPoint offset = layout.isSplit() ? layout.cellOffset(k) : QPoint();
        int column = anchorColumn + offset.x();
        int row = anchorRow + offset.y();
        if (column < 0 || column >= columns || row < 0 || row >= rows)
            return false;
        if (!cells[row * columns + column].isNull())
            return false;
    }

    for (int k = 0; k < count; k++) {
        QPoint offset = layout.isSplit() ? layout.cellOffset(k) : QPoint();
        Screen cell = server;
        cell.setMonitor(layout.isSplit() ? k : -1);
        cells[(anchorRow + offset.y()) * columns + anchorColumn + offset.x()] = cell;
    }
    return true;
}

bool ServerConfig::refreshServerMonitors(const QString& serverName)
{
    m_ServerMonitors = MonitorLayout::fromRects(MonitorLayout::currentMonitorRects());

    // The server's position is the cell of its first monitor, or its only
    // cell if it was not split before.
    int anchor = -1;
    for (std::size_t i = 0; i < screens().size(); i++) {
        const Screen& cell = screens()[i];
        if (cell.isNull() || cell.name() != serverName)
            continue;
        if (anchor == -1 || cell.monitor() == 0)
            anchor = static_cast<int>(i);
    }
    if (anchor == -1)
        return true;

    Screen server = screens()[anchor];
    removeServerCells(screens(), serverName);
    if (placeServerBlock(screens(), numColumns(), numRows(), anchor % numColumns(),
                         anchor / numColumns(), server, m_ServerMonitors)) {
        return true;
    }

    // Another computer sits where a monitor would go. Keep the server as one
    // cell rather than move the user's layout around behind their back.
    m_ServerMonitors = MonitorLayout();
    placeServerBlock(screens(), numColumns(), numRows(), anchor % numColumns(),
                     anchor / numColumns(), server, m_ServerMonitors);
    return false;
}

QString ServerConfig::linkInterval(const Screen& cell, int dirIndex) const
{
    // whole computers, and a server shown as one cell, link along a full edge
    if (!cell.isMonitorCell() || !m_ServerMonitors.isSplit()
            || cell.monitor() >= m_ServerMonitors.count())
        return QString("");

    static const MonitorLayout::Edge edges[] = {
        MonitorLayout::Edge::Right, MonitorLayout::Edge::Left,
        MonitorLayout::Edge::Top, MonitorLayout::Edge::Bottom,
    };
    double start = 0;
    double end = 0;
    if (!m_ServerMonitors.edgeInterval(cell.monitor(), edges[dirIndex], start, end))
        return QString();

    // the config file takes whole percentages, the same rounding the server
    // itself uses when it writes intervals back out
    int from = static_cast<int>(start * 100.0 + 0.5);
    int to = static_cast<int>(end * 100.0 + 0.5);
    if (from >= to)
        return QString();
    if (from == 0 && to == 100)
        return QString("");
    return QString("(%1,%2)").arg(from).arg(to);
}

QTextStream& operator<<(QTextStream& outStream, const ServerConfig& config)
{
    // A server split into monitor cells appears in the grid once per monitor
    // but is one screen to the server, so it is written once.
    outStream << "section: screens\n";

    for (const Screen& s : config.screens()) {
        if (!s.isNull() && s.monitor() <= 0)
            s.writeScreensSection(outStream);
    }

    outStream << "end\n\n";

    outStream << "section: aliases\n";

    for (const Screen& s : config.screens()) {
        if (!s.isNull() && s.monitor() <= 0)
            s.writeAliasesSection(outStream);
    }

    outStream << "end\n\n";

    outStream << "section: links\n";

    static const int opposite[] = { 1, 0, 3, 2 };
    QStringList names;
    QMap<QString, QStringList> links;
    for (std::size_t i = 0; i < config.screens().size(); i++)
    {
        const Screen& cell = config.screens()[i];
        if (cell.isNull())
            continue;
        if (!links.contains(cell.name())) {
            names << cell.name();
            links[cell.name()] = QStringList();
        }

        for (int j = 0; j < static_cast<int>(sizeof(neighbourDirs) / sizeof(neighbourDirs[0])); j++)
        {
            int idx = config.adjacentScreenIndex(static_cast<int>(i),
                                                 neighbourDirs[j].x, neighbourDirs[j].y);
            if (idx == -1 || config.screens()[idx].isNull())
                continue;
            const Screen& neighbour = config.screens()[idx];
            // two monitors of the same server: the cursor moves between them
            // on its own
            if (neighbour.name() == cell.name())
                continue;

            // a null interval means that monitor does not reach the edge, so
            // the cursor can never leave it in that direction
            QString from = config.linkInterval(cell, j);
            QString to = config.linkInterval(neighbour, opposite[j]);
            if (from.isNull() || to.isNull())
                continue;

            links[cell.name()] << QString("\t\t%1%2 = %3%4\n")
                    .arg(neighbourDirs[j].name, from, neighbour.name(), to);
        }
    }

    for (const QString& name : names) {
        outStream << "\t" << name << ":\n";
        for (const QString& line : links[name])
            outStream << line;
    }

    outStream << "end\n\n";

    outStream << "section: options\n";

    if (config.hasHeartbeat())
        outStream << "\t" << "heartbeat = " << config.heartbeat() << "\n";

    outStream << "\t" << "relativeMouseMoves = " << (config.relativeMouseMoves() ? "true" : "false") << "\n";
    outStream << "\t" << "screenSaverSync = " << (config.screenSaverSync() ? "true" : "false") << "\n";
    outStream << "\t" << "win32KeepForeground = " << (config.win32KeepForeground() ? "true" : "false") << "\n";
    outStream << "\t" << "clipboardSharing = " << (config.clipboardSharing() ? "true" : "false") << "\n";
    outStream << "\t" << "clipboardSharingSize = " << config.clipboardSharingSize() << "\n";

    if (config.hasSwitchDelay())
        outStream << "\t" << "switchDelay = " << config.switchDelay() << "\n";

    if (config.hasSwitchDoubleTap())
        outStream << "\t" << "switchDoubleTap = " << config.switchDoubleTap() << "\n";

    outStream << "\t" << "switchCorners = none ";
    for (int i = 0; i < config.switchCorners().size(); i++) {
        auto corner = static_cast<Screen::SwitchCorner>(i);
        if (config.switchCorners()[i]) {
            outStream << "+" << config.switchCornerName(corner) << " ";
        }
    }
    outStream << "\n";

    outStream << "\t" << "switchCornerSize = " << config.switchCornerSize() << "\n";

    for (const Hotkey& hotkey : config.hotkeys()) {
        outStream << hotkey;
    }

    outStream << "end\n\n";

    return outStream;
}

int ServerConfig::numScreens() const
{
    int rval = 0;

    for (const Screen& s : screens()) {
        if (!s.isNull())
            rval++;
    }

    return rval;
}

int ServerConfig::autoAddScreen(const QString name)
{
    int serverIndex = -1;
    int targetIndex = -1;
    if (!findScreenName(m_ServerName, serverIndex)) {
        if (!fixNoServer(m_ServerName, serverIndex)) {
            return kAutoAddScreenManualServer;
        }
    }
    if (findScreenName(name, targetIndex)) {
        // already exists.
        return kAutoAddScreenIgnore;
    }

    int result = showAddClientDialog(name);

    if (result == kAddClientIgnore) {
        return kAutoAddScreenIgnore;
    }

    if (result == kAddClientOther) {
        addToFirstEmptyGrid(name);
        return kAutoAddScreenManualClient;
    }

    bool success = false;
    int startIndex = serverIndex;
    int offset = 1;
    int dirIndex = 0;

    if (result == kAddClientLeft) {
        offset = -1;
        dirIndex = 1;
    }
    else if (result == kAddClientUp) {
        offset = -5;
        dirIndex = 2;
    }
    else if (result == kAddClientDown) {
        offset = 5;
        dirIndex = 3;
    }


    int idx = adjacentScreenIndex(startIndex, neighbourDirs[dirIndex].x,
                    neighbourDirs[dirIndex].y);
    while (idx != -1) {
        if (screens()[idx].isNull()) {
            m_Screens[idx].setName(name);
            success = true;
            break;
        }

        startIndex += offset;
        idx = adjacentScreenIndex(startIndex, neighbourDirs[dirIndex].x,
                    neighbourDirs[dirIndex].y);
    }

    if (!success) {
        addToFirstEmptyGrid(name);
        return kAutoAddScreenManualClient;
    }

    saveSettings();
    return kAutoAddScreenOk;
}

bool ServerConfig::findScreenName(const QString& name, int& index)
{
    bool found = false;
    for (std::size_t i = 0; i < screens().size(); i++) {
        if (!screens()[i].isNull() &&
            screens()[i].name().compare(name) == 0) {
            index = static_cast<int>(i);
            found = true;
            break;
        }
    }
    return found;
}

bool ServerConfig::fixNoServer(const QString& name, int& index)
{
    bool fixed = false;
    if (screens()[serverDefaultIndex].isNull()) {
        m_Screens[serverDefaultIndex].setName(name);
        index = serverDefaultIndex;
        fixed = true;
    }

    return fixed;
}

int ServerConfig::showAddClientDialog(const QString& clientName)
{
    int result = kAddClientIgnore;

    if (!m_pMainWindow->isActiveWindow()) {
        m_pMainWindow->showNormal();
        m_pMainWindow->activateWindow();
    }

    AddClientDialog addClientDialog(clientName, m_pMainWindow);
    addClientDialog.exec();
    result = addClientDialog.addResult();
    m_IgnoreAutoConfigClient = addClientDialog.ignoreAutoConfigClient();

    return result;
}

void::ServerConfig::addToFirstEmptyGrid(const QString &clientName)
{
    for (std::size_t i = 0; i < screens().size(); i++) {
        if (screens()[i].isNull()) {
            m_Screens[i].setName(clientName);
            break;
        }
    }
}

size_t ServerConfig::defaultClipboardSharingSize() {
    return 100 * 1000 * 1000; // 100 MB
}

size_t ServerConfig::setClipboardSharingSize(size_t size) {
    using std::swap;
    swap (size, m_ClipboardSharingSize);
    return size;
}
