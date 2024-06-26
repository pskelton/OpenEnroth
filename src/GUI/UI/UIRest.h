#pragma once

#include "GUI/GUIWindow.h"
#include "Engine/Time/Time.h"

class GUIWindow_Rest : public GUIWindow {
 public:
    /**
     * @offset 0x41F6C1
     */
    GUIWindow_Rest();
    virtual ~GUIWindow_Rest() {}

    /**
     * @offset 0x41FA01
     */
    virtual void Update() override;

 private:
    Duration hourglassLoopTimer;
};

enum class RestType {
    REST_NONE = 0,
    REST_WAIT = 1,
    REST_HEAL = 2
};
using enum RestType;

extern GraphicsImage *rest_ui_sky_frame_current;
extern GraphicsImage *rest_ui_hourglass_frame_current;

extern int foodRequiredToRest;
extern Duration remainingRestTime;
extern RestType currentRestType;
