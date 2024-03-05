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
        this.popupState.popupCheckboxText = uiState.popupState.popupCheckboxText;
        this.popupState.checkboxChecked = uiState.popupState.checkboxChecked;
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
        public String popupCheckboxText = "";
        public boolean checkboxChecked;
        public boolean checkboxFocused;
        public boolean leftButtonPressed;
        public boolean leftButtonFocused;
        public boolean rightButtonPressed;
        public boolean rightButtonFocused;

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            PopupState that = (PopupState) o;
            return showCheckbox == that.showCheckbox && checkboxChecked == that.checkboxChecked && checkboxFocused == that.checkboxFocused && leftButtonPressed == that.leftButtonPressed && leftButtonFocused == that.leftButtonFocused && rightButtonPressed == that.rightButtonPressed && rightButtonFocused == that.rightButtonFocused && Objects.equals(headerText, that.headerText) && Objects.equals(popupText, that.popupText) && Objects.equals(popupButtonLeft, that.popupButtonLeft) && Objects.equals(popupButtonRight, that.popupButtonRight) && Objects.equals(popupCheckboxText, that.popupCheckboxText);
        }

        @Override
        public int hashCode() {
            return Objects.hash(headerText, popupText, showCheckbox, popupButtonLeft, popupButtonRight, popupCheckboxText, checkboxChecked, checkboxFocused, leftButtonPressed, leftButtonFocused, rightButtonPressed, rightButtonFocused);
        }

        @Override
        public String toString() {
            return "PopupState{" +
                    "headerText='" + headerText + '\'' +
                    ", popupText='" + popupText + '\'' +
                    ", showCheckbox=" + showCheckbox +
                    ", popupButtonLeft='" + popupButtonLeft + '\'' +
                    ", popupButtonRight='" + popupButtonRight + '\'' +
                    ", popupCheckboxText='" + popupCheckboxText + '\'' +
                    ", checkboxChecked=" + checkboxChecked +
                    ", checkboxFocused=" + checkboxFocused +
                    ", leftButtonPressed=" + leftButtonPressed +
                    ", leftButtonFocused=" + leftButtonFocused +
                    ", rightButtonPressed=" + rightButtonPressed +
                    ", rightButtonFocused=" + rightButtonFocused +
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
