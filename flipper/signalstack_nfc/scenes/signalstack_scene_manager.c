// Scene manager registration table

#include "signalstack_scene.h"

#define ADD_SCENE(prefix, name, id) \
    [id] = {                        \
        .on_enter = prefix##_scene_##name##_on_enter,  \
        .on_event = prefix##_scene_##name##_on_event,  \
        .on_exit  = prefix##_scene_##name##_on_exit,   \
    },

const SceneManagerHandlers signalstack_scene_handlers = {
    .handlers_count = SignalStackSceneCount,
    .handlers = {
        ADD_SCENE(signalstack, splash,         SignalStackSceneSplash)
        ADD_SCENE(signalstack, main_menu,      SignalStackSceneMainMenu)
        ADD_SCENE(signalstack, scan,           SignalStackSceneScan)
        ADD_SCENE(signalstack, result,         SignalStackSceneResult)
        ADD_SCENE(signalstack, history,        SignalStackSceneHistory)
        ADD_SCENE(signalstack, history_detail, SignalStackSceneHistoryDetail)
        ADD_SCENE(signalstack, settings,       SignalStackSceneSettings)
        ADD_SCENE(signalstack, about,          SignalStackSceneAbout)
    },
};
