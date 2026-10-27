#include <hyprland/src/layout/target/Target.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <optional>

// The controller's saved origin must match the corrected pickup position.
#define private public
#include <hyprland/src/layout/supplementary/DragController.hpp>
#undef private

#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/plugins/HookSystem.hpp>
#include <algorithm>

namespace {
using Controller = Layout::Supplementary::CDragStateController;
CFunctionHook* pickupHook = nullptr;

bool anchoredPickup(Controller* controller) {
    const auto target = controller->target();
    const bool tiled = target && !target->floating() && controller->mode() == MBIND_MOVE;
    const auto window = target ? target->window() : nullptr;
    const auto before = window ? CBox{window->positionAnimation()->value(), window->sizeAnimation()->value()} : CBox{};
    const auto cursor = g_pInputManager->getMouseCoordsInternal();
    const bool rejected = reinterpret_cast<bool (*)(Controller*)>(pickupHook->m_original)(controller);
    if (rejected || !tiled || !controller->draggingTiled() || before.w <= 0 || before.h <= 0)
        return rejected;

    const auto size = target->position().size();
    auto offset = cursor - before.pos();
    if (offset.x >= 0 && offset.x <= before.w) offset.x *= size.x / before.w;
    if (offset.y >= 0 && offset.y <= before.h) offset.y *= size.y / before.h;
    const auto position = cursor - offset;
    target->setPositionGlobal(CBox{position, size});
    target->warpPositionSize();
    controller->m_beginDragPositionXY = position;
    return false;
}
}

void initDragAnchor(HANDLE handle) {
    for (const auto& method : HyprlandAPI::findFunctionsByName(handle, "updateDragWindow")) {
        if (method.signature.find("CDragStateController") == std::string::npos) continue;
        pickupHook = HyprlandAPI::createFunctionHook(handle, method.address, reinterpret_cast<void*>(anchoredPickup));
        if (!pickupHook || !pickupHook->hook()) throw std::runtime_error("hyprbars: cannot preserve drag anchor");
        return;
    }
    throw std::runtime_error("hyprbars: drag controller entry point not found");
}
