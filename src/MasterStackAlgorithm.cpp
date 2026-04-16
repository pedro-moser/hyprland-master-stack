#include "MasterStackAlgorithm.hpp"

#include <hyprland/src/layout/algorithm/Algorithm.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/layout/target/WindowTarget.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>

#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/history/WindowHistoryTracker.hpp>
#include <hyprland/src/helpers/Monitor.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>

#include <hyprutils/string/VarList2.hpp>

extern HANDLE PHANDLE; // defined in main.cpp

using namespace Hyprutils::String;
using namespace Layout;
using namespace Layout::Tiled;

// ─── Constructor / Destructor ───────────────────────────────────────────────

CMasterStackAlgorithm::CMasterStackAlgorithm() {
    const auto PCFG = HyprlandAPI::getConfigValue(PHANDLE, "plugin:master-stack:mfact");
    if (PCFG)
        m_mfact = std::any_cast<Hyprlang::FLOAT>(PCFG->getValue());

    const auto PPEEK = HyprlandAPI::getConfigValue(PHANDLE, "plugin:master-stack:peek_height");
    if (PPEEK)
        m_peekHeight = std::any_cast<Hyprlang::INT>(PPEEK->getValue());

    m_focusCallback = Event::bus()->m_events.window.active.listen([this](PHLWINDOW pWindow, Desktop::eFocusReason reason) {
        if (!pWindow)
            return;

        if (!pWindow->m_workspace || !pWindow->m_workspace->isVisible())
            return;

        const auto TARGET = pWindow->layoutTarget();
        if (!TARGET)
            return;

        const auto NODE = dataFor(TARGET);
        if (!NODE || NODE->isMaster)
            return;

        const int idx = stackIndexOf(NODE);
        if (idx < 0 || idx == m_focusedStackIdx)
            return;

        // peek windows: only respond to click, not hover
        if (reason == Desktop::FOCUS_REASON_FFM) {
            // refocus back to the current focused stack window
            const auto FOCUSED = getFocusedStackNode();
            if (FOCUSED) {
                const auto FT = FOCUSED->target.lock();
                if (FT) {
                    const auto FW = FT->window();
                    if (FW)
                        Desktop::focusState()->fullWindowFocus(FW, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
                }
            }
            return;
        }

        m_focusedStackIdx = idx;
        recalculate();
    });
}

CMasterStackAlgorithm::~CMasterStackAlgorithm() {
    for (const auto& node : m_nodes) {
        const auto TARGET = node->target.lock();
        if (!TARGET)
            continue;
        const auto WINDOW = TARGET->window();
        if (WINDOW)
            WINDOW->setHidden(false);
    }
    m_focusCallback.reset();
}

// ─── Helpers ────────────────────────────────────────────────────────────────

SP<SMasterStackNodeData> CMasterStackAlgorithm::dataFor(SP<ITarget> t) {
    for (auto& node : m_nodes) {
        if (node->target.lock() == t)
            return node;
    }
    return nullptr;
}

SP<SMasterStackNodeData> CMasterStackAlgorithm::getMasterNode() {
    for (auto& node : m_nodes) {
        if (node->isMaster)
            return node;
    }
    return nullptr;
}

SP<SMasterStackNodeData> CMasterStackAlgorithm::getFocusedStackNode() {
    int idx = 0;
    for (auto& node : m_nodes) {
        if (node->isMaster)
            continue;
        if (idx == m_focusedStackIdx)
            return node;
        idx++;
    }
    return nullptr;
}

int CMasterStackAlgorithm::getStackCount() {
    int count = 0;
    for (const auto& node : m_nodes) {
        if (!node->isMaster)
            count++;
    }
    return count;
}

int CMasterStackAlgorithm::stackIndexOf(SP<SMasterStackNodeData> node) {
    int idx = 0;
    for (const auto& n : m_nodes) {
        if (n->isMaster)
            continue;
        if (n == node)
            return idx;
        idx++;
    }
    return -1;
}

void CMasterStackAlgorithm::setFocusedToNode(SP<SMasterStackNodeData> node) {
    const int idx = stackIndexOf(node);
    if (idx >= 0)
        m_focusedStackIdx = idx;
}

// ─── Target Management ─────────────────────────────────────────────────────

void CMasterStackAlgorithm::newTarget(SP<ITarget> target) {
    const bool hasMaster = getMasterNode() != nullptr;

    m_nodes.emplace_back(makeShared<SMasterStackNodeData>(target, !hasMaster));

    if (hasMaster)
        m_focusedStackIdx = getStackCount() - 1;

    recalculate();
}

void CMasterStackAlgorithm::movedTarget(SP<ITarget> target, std::optional<Vector2D> /*focalPoint*/) {
    newTarget(target);
}

void CMasterStackAlgorithm::removeTarget(SP<ITarget> target) {
    auto it = std::ranges::find_if(m_nodes, [target](const auto& node) { return node->target.lock() == target; });

    if (it == m_nodes.end())
        return;

    const bool wasMaster   = (*it)->isMaster;
    const int  removedSIdx = wasMaster ? -1 : stackIndexOf(*it);

    m_nodes.erase(it);

    if (m_nodes.empty()) {
        m_focusedStackIdx = 0;
        return;
    }

    if (wasMaster) {
        m_nodes.front()->isMaster = true;
        m_focusedStackIdx = 0;
    } else {
        const int stackCount = getStackCount();
        if (stackCount == 0) {
            m_focusedStackIdx = 0;
        } else if (removedSIdx <= m_focusedStackIdx) {
            m_focusedStackIdx = std::max(0, m_focusedStackIdx - 1);
        }
    }

    recalculate();
}

// ─── Core Layout (Card Stack) ──────────────────────────────────────────────

void CMasterStackAlgorithm::recalculate() {
    if (m_nodes.empty())
        return;

    const auto WORK_AREA  = m_parent->space()->workArea();
    const int  stackCount = getStackCount();
    int        stackIdx   = 0;

    for (auto& node : m_nodes) {
        const auto TARGET = node->target.lock();
        if (!TARGET)
            continue;

        const auto WINDOW = TARGET->window();
        if (!WINDOW)
            continue;

        if (node->isMaster) {
            CBox masterBox = WORK_AREA;
            if (stackCount > 0)
                masterBox.w = WORK_AREA.w * m_mfact;

            TARGET->setPositionGlobal(masterBox);
            WINDOW->setHidden(false);
        } else {
            const double stackX = WORK_AREA.x + WORK_AREA.w * m_mfact;
            const double stackW = WORK_AREA.w * (1.0f - m_mfact);
            const double stackH = WORK_AREA.h;
            const double stackY = WORK_AREA.y;

            if (stackCount <= 1) {
                CBox box;
                box.x = stackX;
                box.w = stackW;
                box.y = stackY;
                box.h = stackH;
                TARGET->setPositionGlobal(box);
                WINDOW->setHidden(false);
            } else {
                const double peekH = std::min(static_cast<double>(m_peekHeight), stackH / 3.0);
                const bool hasPrev = m_focusedStackIdx > 0;
                const bool hasNext = m_focusedStackIdx < stackCount - 1;
                const int  peekCount = (hasPrev ? 1 : 0) + (hasNext ? 1 : 0);
                const double focusedH = stackH - peekCount * peekH;

                if (stackIdx == m_focusedStackIdx - 1) {
                    // previous neighbor: peek at top, render full-size but clip to peek
                    STargetBox tb;
                    tb.logicalBox = {stackX, stackY, stackW, peekH};
                    tb.visualBox  = {stackX, stackY, stackW, focusedH};
                    TARGET->setPositionGlobal(tb);
                    WINDOW->setHidden(false);
                } else if (stackIdx == m_focusedStackIdx) {
                    // focused: large card
                    const double y = stackY + (hasPrev ? peekH : 0);
                    CBox box = {stackX, y, stackW, focusedH};
                    TARGET->setPositionGlobal(box);
                    WINDOW->setHidden(false);
                } else if (stackIdx == m_focusedStackIdx + 1) {
                    // next neighbor: peek at bottom, show bottom of window
                    const double y = stackY + stackH - peekH;
                    STargetBox tb;
                    tb.logicalBox = {stackX, y, stackW, peekH};
                    tb.visualBox  = {stackX, y + peekH - focusedH, stackW, focusedH};
                    TARGET->setPositionGlobal(tb);
                    WINDOW->setHidden(false);
                } else {
                    WINDOW->setHidden(true);
                }
            }

            stackIdx++;
        }
    }
}

void CMasterStackAlgorithm::resizeTarget(const Vector2D& delta, SP<ITarget> target, eRectCorner /*corner*/) {
    const auto NODE = dataFor(target);
    if (!NODE)
        return;

    const auto WORK_AREA = m_parent->space()->workArea();
    if (WORK_AREA.w <= 0)
        return;

    if (delta.x == 0)
        return;

    const float sign      = NODE->isMaster ? 1.0f : -1.0f;
    const float deltaFact = static_cast<float>(delta.x / WORK_AREA.w);
    m_mfact = std::clamp(m_mfact + sign * deltaFact, 0.05f, 0.95f);

    recalculate();
}

// ─── Navigation & Swap ──────────────────────────────────────────────────────

SP<ITarget> CMasterStackAlgorithm::getNextCandidate(SP<ITarget> old) {
    if (m_nodes.empty())
        return nullptr;

    const auto MASTER   = getMasterNode();
    const auto OLD_NODE = dataFor(old);

    if (OLD_NODE && OLD_NODE->isMaster) {
        const auto STACK = getFocusedStackNode();
        return STACK ? STACK->target.lock() : nullptr;
    }

    // stack window removed: prefer adjacent stack window
    SP<SMasterStackNodeData> prev = nullptr;
    bool                       foundOld = false;

    for (auto& node : m_nodes) {
        if (node->isMaster)
            continue;
        if (node == OLD_NODE) {
            foundOld = true;
            continue;
        }
        if (foundOld)
            return node->target.lock(); // next stack neighbor
        prev = node;
    }

    if (prev)
        return prev->target.lock(); // previous stack neighbor

    // no other stack windows, fall back to master
    return MASTER ? MASTER->target.lock() : nullptr;
}

void CMasterStackAlgorithm::swapTargets(SP<ITarget> a, SP<ITarget> b) {
    auto nodeA = dataFor(a);
    auto nodeB = dataFor(b);

    if (!nodeA || !nodeB)
        return;

    nodeA->target = b;
    nodeB->target = a;

    recalculate();
}

void CMasterStackAlgorithm::moveTargetInDirection(SP<ITarget> t, Math::eDirection dir, bool silent) {
    static auto PMONITORFALLBACK = CConfigValue<Hyprlang::INT>("binds:window_direction_monitor_fallback");

    const auto NODE = dataFor(t);
    if (!NODE)
        return;

    if (NODE->isMaster && dir == Math::eDirection::DIRECTION_RIGHT) {
        const auto STACK = getFocusedStackNode();
        if (STACK) {
            NODE->isMaster  = false;
            STACK->isMaster = true;
            setFocusedToNode(NODE);
            recalculate();
            if (!silent) {
                const auto TARGET = NODE->target.lock();
                if (TARGET && TARGET->window())
                    Desktop::focusState()->fullWindowFocus(TARGET->window(), Desktop::FOCUS_REASON_KEYBIND);
            }
            return;
        }
    }

    if (!NODE->isMaster && dir == Math::eDirection::DIRECTION_LEFT) {
        const auto MASTER = getMasterNode();
        if (MASTER) {
            MASTER->isMaster = false;
            NODE->isMaster   = true;
            setFocusedToNode(MASTER);
            recalculate();
            if (!silent) {
                const auto TARGET = NODE->target.lock();
                if (TARGET && TARGET->window())
                    Desktop::focusState()->fullWindowFocus(TARGET->window(), Desktop::FOCUS_REASON_KEYBIND);
            }
            return;
        }
    }

    if (!*PMONITORFALLBACK)
        return;

    if (!t || !t->space() || !t->space()->workspace())
        return;

    const auto PMONINDIR = g_pCompositor->getMonitorInDirection(t->space()->workspace()->m_monitor.lock(), dir);

    if (PMONINDIR && PMONINDIR != t->space()->workspace()->m_monitor.lock()) {
        const auto TARGETWS = PMONINDIR->m_activeWorkspace;
        if (t->window())
            t->window()->setAnimationsToMove();
        t->assignToSpace(TARGETWS->m_space, focalPointForDir(t, dir));
    }
}

// ─── Layout Messages ────────────────────────────────────────────────────────

std::expected<void, std::string> CMasterStackAlgorithm::layoutMsg(const std::string_view& sv) {
    CVarList2 vars(std::string{sv}, 0, 's');

    if (vars.size() < 1 || vars[0].empty())
        return std::unexpected("layoutmsg requires at least 1 argument");

    const auto COMMAND = vars[0];
    const auto PWINDOW = Desktop::focusState()->window();

    auto switchToWindow = [](SP<ITarget> target) {
        if (!target)
            return;
        const auto WINDOW = target->window();
        if (!WINDOW || !Desktop::View::validMapped(WINDOW))
            return;
        Desktop::focusState()->fullWindowFocus(WINDOW, Desktop::FOCUS_REASON_KEYBIND);
        g_pCompositor->warpCursorTo(target->position().middle());
    };

    if (COMMAND == "cyclenext") {
        cycleNext();
        return {};
    } else if (COMMAND == "cycleprev") {
        cyclePrev();
        return {};
    } else if (COMMAND == "swapwithmaster") {
        if (!PWINDOW)
            return std::unexpected("no focused window");

        const auto MASTER = getMasterNode();
        if (!MASTER)
            return std::unexpected("no master");

        const auto NODE = dataFor(PWINDOW->layoutTarget());
        if (!NODE)
            return std::unexpected("window not in layout");

        if (NODE->isMaster) {
            const auto STACK = getFocusedStackNode();
            if (!STACK)
                return std::unexpected("no stack window");

            MASTER->isMaster = false;
            STACK->isMaster  = true;
            setFocusedToNode(MASTER);

            recalculate();

            const bool focusChild = vars.size() >= 2 && vars[1] == "child";
            switchToWindow(focusChild ? MASTER->target.lock() : STACK->target.lock());
        } else {
            MASTER->isMaster = false;
            NODE->isMaster   = true;
            setFocusedToNode(MASTER);

            recalculate();

            const bool focusChild = vars.size() >= 2 && vars[1] == "child";
            switchToWindow(focusChild ? MASTER->target.lock() : NODE->target.lock());
        }

        return {};
    } else if (COMMAND == "focusmaster") {
        const auto MASTER = getMasterNode();
        if (!MASTER)
            return std::unexpected("no master");

        if (PWINDOW && PWINDOW->layoutTarget() == MASTER->target.lock()) {
            const auto STACK = getFocusedStackNode();
            if (STACK)
                switchToWindow(STACK->target.lock());
        } else {
            switchToWindow(MASTER->target.lock());
        }

        return {};
    } else if (COMMAND == "mfact") {
        const bool exact = vars.size() >= 2 && vars[1] == "exact";

        float ratio = 0.f;
        try {
            ratio = std::stof(std::string{exact ? vars[2] : vars[1]});
        } catch (...) {
            return std::unexpected("bad ratio");
        }

        m_mfact = std::clamp(exact ? ratio : m_mfact + ratio, 0.05f, 0.95f);

        recalculate();
        return {};
    }

    return std::unexpected(std::format("Unknown master-stack layoutmsg: {}", COMMAND));
}

std::optional<Vector2D> CMasterStackAlgorithm::predictSizeForNewTarget() {
    const auto WORK_AREA = m_parent->space()->workArea();

    if (m_nodes.empty())
        return WORK_AREA.size();

    const int  futureStackCount = getStackCount() + 1;
    const double peekH    = std::min(static_cast<double>(m_peekHeight), WORK_AREA.h / futureStackCount);
    const double focusedH = WORK_AREA.h - (futureStackCount - 1) * peekH;

    return Vector2D{WORK_AREA.w * (1.0f - m_mfact), std::max(focusedH, peekH)};
}

// ─── Cycle & Focus ─────────────────────────────────────────────────────────

bool CMasterStackAlgorithm::isOnStack() {
    const auto PWINDOW = Desktop::focusState()->window();
    if (!PWINDOW)
        return false;
    const auto NODE = dataFor(PWINDOW->layoutTarget());
    return NODE && !NODE->isMaster;
}

bool CMasterStackAlgorithm::isFirstStack() {
    return m_focusedStackIdx <= 0;
}

bool CMasterStackAlgorithm::isLastStack() {
    return m_focusedStackIdx >= getStackCount() - 1;
}

void CMasterStackAlgorithm::cycleNext() {
    const int stackCount = getStackCount();
    if (stackCount <= 0)
        return;

    m_focusedStackIdx = (m_focusedStackIdx + 1) % stackCount;
    updateFocus();
}

void CMasterStackAlgorithm::cyclePrev() {
    const int stackCount = getStackCount();
    if (stackCount <= 0)
        return;

    m_focusedStackIdx--;
    if (m_focusedStackIdx < 0)
        m_focusedStackIdx = stackCount - 1;
    updateFocus();
}

void CMasterStackAlgorithm::focusTargetUpdate(SP<ITarget> target) {
    const auto NODE = dataFor(target);
    if (!NODE || NODE->isMaster)
        return;

    const int idx = stackIndexOf(NODE);
    if (idx >= 0 && m_focusedStackIdx != idx) {
        m_focusedStackIdx = idx;
        recalculate();
    }
}

void CMasterStackAlgorithm::updateFocus() {
    recalculate();

    const auto STACK = getFocusedStackNode();
    if (!STACK)
        return;

    const auto TARGET = STACK->target.lock();
    if (!TARGET)
        return;

    const auto WINDOW = TARGET->window();
    if (!WINDOW)
        return;

    Desktop::focusState()->fullWindowFocus(WINDOW, Desktop::FOCUS_REASON_DESKTOP_STATE_CHANGE);
}
