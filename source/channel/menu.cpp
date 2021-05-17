/****************************************************************************
 * libwiigui Template
 * Tantric 2009
 *
 * menu.cpp
 * Menu flow routines - handles all menu logic
 ***************************************************************************/

#include <sstream>
#include <unistd.h>

#include "gui/gui.h"
#include "main.h"
#include "menu.h"

#define THREAD_SLEEP 100
// 48 KiB was chosen after many days of testing.
// It horrifies the author.
#define GUI_STACK_SIZE 48 * 1024

static GuiImageData *pointer[4];
static GuiImage *bgImg = NULL;
static GuiSound *bgMusic = NULL;
static GuiWindow *mainWindow = NULL;
static lwp_t guithread = LWP_THREAD_NULL;
static bool guiHalt = true;
static bool ExitRequested = false;


/****************************************************************************
 * ResumeGui
 *
 * Signals the GUI thread to start, and resumes the thread. This is called
 * after finishing the removal/insertion of new elements, and after initial
 * GUI setup.
 ***************************************************************************/
static void ResumeGui() {
    guiHalt = false;
    LWP_ResumeThread(guithread);
}

/****************************************************************************
 * HaltGui
 *
 * Signals the GUI thread to stop, and waits for GUI thread to stop
 * This is necessary whenever removing/inserting new elements into the GUI.
 * This eliminates the possibility that the GUI is in the middle of accessing
 * an element that is being changed.
 ***************************************************************************/
static void HaltGui() {
    guiHalt = true;

    // wait for thread to finish
    while (!LWP_ThreadIsSuspended(guithread))
        usleep(THREAD_SLEEP);
}

/****************************************************************************
 * UpdateGUI
 *
 * Primary thread to allow GUI to respond to state changes, and draws GUI
 ***************************************************************************/

static void *UpdateGUI(void *arg) {
    int i;

    while (1) {
        if (guiHalt) {
            LWP_SuspendThread(guithread);
        } else {
            UpdatePads();
            mainWindow->Draw();

            // so that player 1's cursor appears on top!
            for (i = 3; i >= 0; i--) {
                if (userInput[i].wpad->ir.valid)
                    Menu_DrawImg(userInput[i].wpad->ir.x - 48,
                                 userInput[i].wpad->ir.y - 48, 96, 96,
                                 pointer[i]->GetImage(),
                                 userInput[i].wpad->ir.angle, 1, 1, 255);
                DoRumble(i);
            }

            Menu_Render();

            for (i = 0; i < 4; i++)
                mainWindow->Update(&userInput[i]);

            if (ExitRequested) {
                for (i = 0; i <= 255; i += 15) {
                    mainWindow->Draw();
                    Menu_DrawRectangle(0, 0, screenwidth, screenheight,
                                       (GXColor){0, 0, 0, (u8)i}, 1);
                    Menu_Render();
                }
                ExitApp();
            }
        }
    }
    return NULL;
}

/****************************************************************************
 * InitGUIThread
 *
 * Startup GUI threads
 ***************************************************************************/
static u8 *_gui_stack[GUI_STACK_SIZE] ATTRIBUTE_ALIGN(8);
void InitGUIThreads() {
    LWP_CreateThread(&guithread, UpdateGUI, NULL, _gui_stack, GUI_STACK_SIZE,
                     70);
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
void OnScreenKeyboard(wchar_t *var, u16 maxlen, const char* name) {
    int save = -1;

    GuiKeyboard keyboard(var, maxlen);


    GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
    GuiImageData btnOutline(button_png);
    GuiImageData btnOutlineOver(button_over_png);
    GuiTrigger trigA;
    trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A,
                           PAD_BUTTON_A);

    GuiText titleTxt(name, 28, (GXColor){255, 255, 255, 255});
    titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
    titleTxt.SetPosition(0, 25);

    GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 255});
    GuiImage okBtnImg(&btnOutline);
    GuiImage okBtnImgOver(&btnOutlineOver);
    GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

    okBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
    okBtn.SetPosition(-50, 25);

    okBtn.SetLabel(&okBtnTxt);
    okBtn.SetImage(&okBtnImg);
    okBtn.SetImageOver(&okBtnImgOver);
    okBtn.SetSoundOver(&btnSoundOver);
    okBtn.SetTrigger(&trigA);
    okBtn.SetEffectGrow();

    GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
    GuiImage cancelBtnImg(&btnOutline);
    GuiImage cancelBtnImgOver(&btnOutlineOver);
    GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
    cancelBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
    cancelBtn.SetPosition(50, 25);
    cancelBtn.SetLabel(&cancelBtnTxt);
    cancelBtn.SetImage(&cancelBtnImg);
    cancelBtn.SetImageOver(&cancelBtnImgOver);
    cancelBtn.SetSoundOver(&btnSoundOver);
    cancelBtn.SetTrigger(&trigA);
    cancelBtn.SetEffectGrow();

    keyboard.Append(&okBtn);
    keyboard.Append(&cancelBtn);

    HaltGui();
    mainWindow->SetState(STATE_DISABLED);
    mainWindow->Append(&keyboard);
    mainWindow->Append(&titleTxt);
    mainWindow->ChangeFocus(&keyboard);
    ResumeGui();

    while (save == -1) {
        usleep(THREAD_SLEEP);

        if (okBtn.GetState() == STATE_CLICKED)
            save = 1;
        else if (cancelBtn.GetState() == STATE_CLICKED)
            save = 0;
    }

    // Convert wchar_t to std::string
    std::wstring ws(keyboard.kbtextstr);
    std::string str(ws.begin(), ws.end());

    if (save == 1) {
        std::stringstream ss;
        ss << "/startup?initpage=showTitle&titleId=" << str;
        WII_LaunchTitleWithArgs(0x0001000248414241LL, 0, ss.str().c_str(), NULL);
    }

    if (save == 0) {
        ExitApp();
    }

    HaltGui();
    mainWindow->Remove(&keyboard);
    mainWindow->Remove(&titleTxt);
    mainWindow->SetState(STATE_DEFAULT);
    ResumeGui();
}

/****************************************************************************
 * KeyboardDataEntry
 ***************************************************************************/

static int KeyboardDataEntry(wchar_t *input, const char* name) {
    int menu = MENU_NONE;

    HaltGui();
    GuiWindow w(screenwidth, screenheight);
    mainWindow->Append(&w);
    ResumeGui();

    while (menu == MENU_NONE) {
        usleep(THREAD_SLEEP);

        OnScreenKeyboard(input, 255, name);
        menu = MENU_PRIMARY;
    }

    HaltGui();

    mainWindow->Remove(&w);
    return menu;
}

/****************************************************************************
 * MainMenu
 ***************************************************************************/
void MainMenu(int menu) {
    int currentMenu = menu;

    wchar_t baseText[32] = L"Enter the title ID of the game";

    pointer[0] = new GuiImageData(player1_point_png);
    pointer[1] = new GuiImageData(player2_point_png);
    pointer[2] = new GuiImageData(player3_point_png);
    pointer[3] = new GuiImageData(player4_point_png);

    mainWindow = new GuiWindow(screenwidth, screenheight);

    bgImg = new GuiImage(screenwidth, screenheight, (GXColor){0, 0, 0, 255});

    // Create a white stripe beneath the title and above actionable buttons.
    bgImg->ColorStripe(75, (GXColor){0xff, 0xff, 0xff, 255});
    bgImg->ColorStripe(76, (GXColor){0xff, 0xff, 0xff, 255});

    bgImg->ColorStripe(screenheight - 77, (GXColor){0xff, 0xff, 0xff, 255});
    bgImg->ColorStripe(screenheight - 78, (GXColor){0xff, 0xff, 0xff, 255});

    GuiImage *topChannelGradient =
        new GuiImage(new GuiImageData(channel_gradient_top_png));
    topChannelGradient->SetTile(screenwidth / 4);

    GuiImage *bottomChannelGradient =
        new GuiImage(new GuiImageData(channel_gradient_bottom_png));
    bottomChannelGradient->SetTile(screenwidth / 4);
    bottomChannelGradient->SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);

    mainWindow->Append(bgImg);
    mainWindow->Append(topChannelGradient);
    mainWindow->Append(bottomChannelGradient);

    GuiTrigger trigA;
    trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A,
                           PAD_BUTTON_A);

    ResumeGui();

    // bgMusic = new GuiSound(bg_music_ogg, bg_music_ogg_size, SOUND_OGG);
    // bgMusic->SetVolume(50);
    // bgMusic->Play(); // startup music

    while (currentMenu != MENU_EXIT) {
        switch (currentMenu) {
        case MENU_PRIMARY:
            currentMenu = KeyboardDataEntry(baseText, "Shop Page Loader");
            break;
        default:
            currentMenu = KeyboardDataEntry(baseText, "Shop Page Loader");
            break;
        }
    }

    ResumeGui();
    ExitRequested = true;
    while (1)
        usleep(THREAD_SLEEP);

    HaltGui();

    bgMusic->Stop();
    delete bgMusic;
    delete bgImg;
    delete mainWindow;

    delete pointer[0];
    delete pointer[1];
    delete pointer[2];
    delete pointer[3];

    mainWindow = NULL;
}
