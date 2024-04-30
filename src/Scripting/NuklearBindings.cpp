#include "NuklearBindings.h"

#include <Engine/Graphics/Nuklear.h>

NuklearBindings::NuklearBindings(Nuklear *nuklear) : _nuklear(nuklear) {
}

sol::table NuklearBindings::createBindingTable(sol::state_view &solState) const {
    _nuklear->setLuaState(solState);
    _nuklear->initBindings();
    //We already have a lot of bindings done without sol2. It's going to take a bit before we move everything here.
    return solState.create_table();
}