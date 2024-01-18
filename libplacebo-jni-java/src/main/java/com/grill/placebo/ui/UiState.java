package com.grill.placebo.ui;

import java.util.Objects;

public class UiState {

    public boolean showTouchpad;

    public boolean showPanel;

    public boolean showPopup;

    public boolean touchpadPressed;

    public boolean panelPressed;

    public final PanelState panelState = new PanelState();

    public final PopupState popupState = new PopupState();

    public void clone(UiState uiState) {
        this.showTouchpad = uiState.showTouchpad;
        this.showPanel = uiState.showPanel;
        this.showPopup = uiState.showPopup;
        this.touchpadPressed = uiState.touchpadPressed;
        this.panelPressed = uiState.panelPressed;
        // panel state
        this.panelState.showMicButton = uiState.panelState.showMicButton;
        this.panelState.micButtonPressed = uiState.panelState.micButtonPressed;
        this.panelState.micButtonActive = uiState.panelState.micButtonActive;
        this.panelState.shareButtonPressed = uiState.panelState.shareButtonPressed;
        this.panelState.psButtonPressed = uiState.panelState.psButtonPressed;
        this.panelState.optionsButtonPressed = uiState.panelState.optionsButtonPressed;
        this.panelState.fullscreenButtonPressed = uiState.panelState.fullscreenButtonPressed;
        this.panelState.fullscreenButtonActive = uiState.panelState.fullscreenButtonActive;
        this.panelState.closeButtonPressed = uiState.panelState.closeButtonPressed;
        // popup state
        this.popupState.headerText = uiState.popupState.headerText;
        this.popupState.popupText = uiState.popupState.popupText;
        this.popupState.showCheckbox = uiState.popupState.showCheckbox;
        this.popupState.popupButtonLeft = uiState.popupState.popupButtonLeft;
        this.popupState.popupButtonRight = uiState.popupState.popupButtonRight;
        this.popupState.checkboxPressed = uiState.popupState.checkboxPressed;
        this.popupState.checkboxFocused = uiState.popupState.checkboxFocused;
        this.popupState.leftButtonPressed = uiState.popupState.leftButtonPressed;
        this.popupState.leftButtonFocused = uiState.popupState.leftButtonFocused;
        this.popupState.rightButtonPressed = uiState.popupState.rightButtonPressed;
        this.popupState.rightButtonFocused = uiState.popupState.rightButtonFocused;
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || this.getClass() != o.getClass()) return false;
        UiState uiState = (UiState) o;
        // no touchpadPressed and panelPressed as these are just internal config states
        return this.showTouchpad == uiState.showTouchpad && this.showPanel == uiState.showPanel && this.showPopup == uiState.showPopup && Objects.equals(this.panelState, uiState.panelState) && Objects.equals(this.popupState, uiState.popupState);
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.showTouchpad, this.showPanel, this.showPopup, this.panelState, this.popupState);
    }

    @Override
    public String toString() {
        return "UiState{" +
                "showTouchpad=" + this.showTouchpad +
                ", showPanel=" + this.showPanel +
                ", showPopup=" + this.showPopup +
                ", panelState=" + this.panelState +
                ", popupState=" + this.popupState +
                '}';
    }

    /*********************/
    /*** inner classes ***/
    /*********************/

    public static class PanelState {
        public boolean showMicButton = true;
        public boolean micButtonPressed;
        public boolean micButtonActive;
        public boolean shareButtonPressed;
        public boolean psButtonPressed;
        public boolean optionsButtonPressed;
        public boolean fullscreenButtonPressed;
        public boolean fullscreenButtonActive;
        public boolean closeButtonPressed;

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || this.getClass() != o.getClass()) return false;
            PanelState that = (PanelState) o;
            return this.showMicButton == that.showMicButton && this.micButtonPressed == that.micButtonPressed && this.micButtonActive == that.micButtonActive && this.shareButtonPressed == that.shareButtonPressed && this.psButtonPressed == that.psButtonPressed && this.optionsButtonPressed == that.optionsButtonPressed && this.fullscreenButtonPressed == that.fullscreenButtonPressed && this.fullscreenButtonActive == that.fullscreenButtonActive && this.closeButtonPressed == that.closeButtonPressed;
        }

        @Override
        public int hashCode() {
            return Objects.hash(this.showMicButton, this.micButtonPressed, this.micButtonActive, this.shareButtonPressed, this.psButtonPressed, this.optionsButtonPressed, this.fullscreenButtonPressed, this.fullscreenButtonActive, this.closeButtonPressed);
        }

        @Override
        public String toString() {
            return "PanelState{" +
                    "showMicButton=" + this.showMicButton +
                    ", micButtonPressed=" + this.micButtonPressed +
                    ", micButtonActive=" + this.micButtonActive +
                    ", shareButtonPressed=" + this.shareButtonPressed +
                    ", psButtonPressed=" + this.psButtonPressed +
                    ", optionsButtonPressed=" + this.optionsButtonPressed +
                    ", fullscreenButtonPressed=" + this.fullscreenButtonPressed +
                    ", fullscreenButtonActive=" + this.fullscreenButtonActive +
                    ", closeButtonPressed=" + this.closeButtonPressed +
                    '}';
        }
    }

    public static class PopupState {
        public String headerText = "";
        public String popupText = "";
        public boolean showCheckbox;
        public String popupButtonLeft = "";
        public String popupButtonRight = "";
        public boolean checkboxPressed;
        public boolean checkboxFocused;
        public boolean leftButtonPressed;
        public boolean leftButtonFocused;
        public boolean rightButtonPressed;
        public boolean rightButtonFocused;

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || this.getClass() != o.getClass()) return false;
            PopupState that = (PopupState) o;
            return this.showCheckbox == that.showCheckbox && this.checkboxPressed == that.checkboxPressed && this.checkboxFocused == that.checkboxFocused && this.leftButtonPressed == that.leftButtonPressed && this.leftButtonFocused == that.leftButtonFocused && this.rightButtonPressed == that.rightButtonPressed && this.rightButtonFocused == that.rightButtonFocused && Objects.equals(this.headerText, that.headerText) && Objects.equals(this.popupText, that.popupText) && Objects.equals(this.popupButtonLeft, that.popupButtonLeft) && Objects.equals(this.popupButtonRight, that.popupButtonRight);
        }

        @Override
        public int hashCode() {
            return Objects.hash(this.headerText, this.popupText, this.showCheckbox, this.popupButtonLeft, this.popupButtonRight, this.checkboxPressed, this.checkboxFocused, this.leftButtonPressed, this.leftButtonFocused, this.rightButtonPressed, this.rightButtonFocused);
        }

        @Override
        public String toString() {
            return "PopupState{" +
                    "headerText='" + this.headerText + '\'' +
                    ", popupText='" + this.popupText + '\'' +
                    ", showCheckbox=" + this.showCheckbox +
                    ", popupButtonLeft='" + this.popupButtonLeft + '\'' +
                    ", popupButtonRight='" + this.popupButtonRight + '\'' +
                    ", checkboxPressed=" + this.checkboxPressed +
                    ", checkboxFocused=" + this.checkboxFocused +
                    ", leftButtonPressed=" + this.leftButtonPressed +
                    ", leftButtonFocused=" + this.leftButtonFocused +
                    ", rightButtonPressed=" + this.rightButtonPressed +
                    ", rightButtonFocused=" + this.rightButtonFocused +
                    '}';
        }
    }

    /******************/
    /*** inner enum ***/
    /******************/

    public enum UiButton {
        MIC_BUTTON,
        SHARE_BUTTON,
        PS_BUTTON,
        OPTIONS_BUTTON,
        FULLSCREEN_BUTTON,
        CLOSE_BUTTON,
        TOUCHPAD_BUTTON,
        DIALOG_BUTTON_LEFT,
        DIALOG_BUTTON_RIGHT,
        DIALOG_CHECKBOX_BUTTON

    }
}
