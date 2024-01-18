// used to indicate the current ui state
UiState globalUiState;
unsigned long previousUiStateId = 0;
unsigned long currentUiStateId = 0;

struct PanelState {
    bool showMicButton;
    bool micButtonPressed;
    bool shareButtonPressed;
    bool psButtonPressed;
    bool optionsButtonPressed;
    bool fullscreenButtonPressed;
    bool closeButtonPressed;
};

struct PopupState {
    const char* headerText;
    const char* popupText;
    bool showCheckbox;
    const char* popupButtonLeft;
    const char* popupButtonRight;
    bool checkboxPressed;
    bool leftPressed;
    bool rightPressed;
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