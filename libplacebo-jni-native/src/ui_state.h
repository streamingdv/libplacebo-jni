// The largest code the account picker draws, along each edge and quiet zone included, which is more than a
// login link ever needs. Here rather than with the drawing of it, as it is what sizes the state.
#define PLAYER_PICKER_MAX_QR_MODULES 64

struct PanelState {
    bool showMicButton;
    bool showFullscreenButton;
    bool showAspectButton;
    bool showVolumeButtons;         // the pair that moves the volume of the system, see volume_icons.h
    bool micButtonPressed;
    bool micButtonActive;
    bool shareButtonPressed;
    bool psButtonPressed;
    bool optionsButtonPressed;
    bool fullscreenButtonPressed;
    bool fullscreenButtonActive;
    bool closeButtonPressed;
    bool aspectButtonPressed;
    bool volumeDownPressed;
    bool volumeUpPressed;
    int aspectModeIndex;    // which video format the button shows, see aspect_icons.h
};

struct PopupState {
    const char* headerText;
    const char* popupText;
    bool showCheckbox;
    const char* popupButtonLeft;
    const char* popupButtonRight;
    const char* checkboxText;
    bool checkboxChecked;
    bool checkboxFocused;
    bool leftButtonPressed;
    bool leftButtonFocused;
    bool rightButtonPressed;
    bool rightButtonFocused;
};

// The card a joining player picks the account they join with on, and types the passcode of that account
// into where the console asks for one, see player_picker.h. Which tile carries the highlight is the app's
// answer and not the renderer's: the controller that asked to join is what moves it, and the mouse moves
// it as well, so this only ever says where to draw the ring.
//
// Buffers of their own rather than pointers, for the reason the lines of UiState below have them: the java
// side writes this state without a lock while the render thread reads it.
struct PlayerPickerState {
    int page;                   // the accounts, the code a phone reads, the passcode, or the wait after it
    int focused;                // which tile the ring is on, the new account one after the last account
    int accountCount;
    char title[128];
    char message[256];          // the line above the code, on that page only
    char hint[256];             // that the console has to hold the account as a user of its own
    char accountNames[256];     // newline separated and in tile order
    char monograms[64];         // the letters of their circles, newline separated and in the same order
    char newAccountText[64];
    char cancelText[64];
    char backText[64];
    bool showBackButton;
    bool cancelPressed;
    bool backPressed;
    int qrModuleCount;          // along each edge of the code, quiet zone included, 0 while there is none
    // One byte per module of it, row by row and non zero for a dark one.
    unsigned char qrModules[PLAYER_PICKER_MAX_QR_MODULES * PLAYER_PICKER_MAX_QR_MODULES];
    // How many digits of the passcode have been typed, on that page only: which boxes are filled and which
    // one of them is lit. A count and never the digits, which are the app's and stay there.
    int typedDigits;
};

struct UiState {
    const char* notStreamableText;
    bool showTouchpad;
    bool showPanel;
    bool showPopup;
    bool touchpadPressed;
    bool panelPressed;
    // The share of the window width the touchpad spans, see touchpadRect of ui_consts.h. Set for the
    // session rather than per frame, and zero until a java side that knows this setting names it.
    float touchpadWidthFraction;
    bool showContentNotStreamable;
    bool showPerfOverlay;           // the performance overlay, only for a session that has it enabled
    bool perfOverlayCollapsed;      // folded away to the pill, for the rest of the session
    bool perfOverlayClosePressed;
    bool perfOverlayArrowPressed;
    // The line the app has built, see perf_overlay.h. A buffer of its own rather than a pointer, because
    // the java side writes this state without a lock while the render thread reads it, and a line that
    // arrives twice a second must not be able to hand that thread memory that has just been freed.
    char perfOverlayText[192];
    bool showJoinHint;              // the hint naming the button a player joins with, see join_hint.h
    char joinHintText[192];         // its sentence, in a buffer of its own for the reason the line above has one
    int padOverlaySeats;            // how many player seats the controller overlay shows, see pad_overlay.h
    int padOverlayConnectedMask;    // bit per seat, set while a controller is holding it
    int padOverlayJoinedMask;       // bit per seat, set once the console has let that player in
    // The seats' names, newline separated and in seat order, in a buffer of its own for the same reason as
    // the two above. Four short labels, so the room here is what four of them and their separators need.
    char padOverlayNames[256];
    bool padOverlayHold;            // show the overlay with the panel down, the app holding it up a moment
    bool showPlayerPicker;          // the card a joining player picks an account on, see player_picker.h
    PanelState panelState;
    PopupState popupState;
    PlayerPickerState playerPickerState;
};

// used to indicate the current ui state
UiState globalUiState;
unsigned long previousUiStateId = 0;
unsigned long currentUiStateId = 0;