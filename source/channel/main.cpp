#include <gccore.h>
#include <wiiuse/wpad.h>

// GUI
#include "gui/gui.h"
#include "menu.h"
#include "noto_sans_jp_regular_otf.h"

#define TITLE_ID(x,y)		(((u64)(x) << 32) | (y))

static void power_cb() {
    ShutoffRumble();
    StopGX();
    STM_ShutdownToIdle();
}

void ExitApp() {
    ShutoffRumble();
    StopGX();
    WII_ReturnToMenu();
}


static void reset_cb(u32 level, void *unk) { ExitApp(); }

int main(void) {
    // Make hardware buttons functional.
    SYS_SetPowerCallback(power_cb);
    SYS_SetResetCallback(reset_cb);

    InitVideo();

    ISFS_Initialize();
    CONF_Init();
    SetupPads();
    InitAudio();
    InitFreeType((u8 *)noto_sans_jp_regular_otf, noto_sans_jp_regular_otf_size);
    InitGUIThreads();


    MainMenu(1);

    return 0;
}
