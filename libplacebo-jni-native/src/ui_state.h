struct PanelState {
    bool showMicButton;
    bool micButtonPressed;
    bool micButtonActive;
    bool shareButtonPressed;
    bool psButtonPressed;
    bool optionsButtonPressed;
    bool fullscreenButtonPressed;
    bool fullscreenButtonActive;
    bool closeButtonPressed;
};

struct PopupState {
    const char* headerText;
    const char* popupText;
    bool showCheckbox;
    const char* popupButtonLeft;
    const char* popupButtonRight;
    bool checkboxPressed;
    bool checkboxFocused;
    bool leftButtonPressed;
    bool leftButtonFocused;
    bool rightButtonPressed;
    bool rightButtonFocused;
};

struct UiState {
    bool showTouchpad;
    bool showPanel;
    bool showPopup;
    bool touchpadPressed;
    bool panelPressed;
    PanelState panelState;
    PopupState popupState;
};

// used to indicate the current ui state
UiState globalUiState;
unsigned long previousUiStateId = 0;
unsigned long currentUiStateId = 0;