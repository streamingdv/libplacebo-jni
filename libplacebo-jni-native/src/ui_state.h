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

struct UiState {
    const char* notStreamableText;
    bool showTouchpad;
    bool showPanel;
    bool showPopup;
    bool touchpadPressed;
    bool panelPressed;
    bool showContentNotStreamable;
    bool showPerfOverlay;           // the performance overlay, only for a session that has it enabled
    bool perfOverlayCollapsed;      // folded away to the pill, for the rest of the session
    bool perfOverlayClosePressed;
    bool perfOverlayArrowPressed;
    // The line the app has built, see perf_overlay.h. A buffer of its own rather than a pointer, because
    // the java side writes this state without a lock while the render thread reads it, and a line that
    // arrives twice a second must not be able to hand that thread memory that has just been freed.
    char perfOverlayText[192];
    PanelState panelState;
    PopupState popupState;
};

// used to indicate the current ui state
UiState globalUiState;
unsigned long previousUiStateId = 0;
unsigned long currentUiStateId = 0;