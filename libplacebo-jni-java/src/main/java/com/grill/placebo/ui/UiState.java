package com.grill.placebo.ui;

import java.util.Objects;

public class UiState {

    public boolean showTouchpad;

    public boolean showPanel;

    public boolean showPopup;

    public boolean touchpadPressed;

    public boolean panelPressed;

    public boolean showContentNotStreamable;

    public String contentNotStreamableText = "";

    /**
     * The performance overlay of the stream, which only a session that has it enabled shows.
     */
    public boolean showPerfOverlay;

    /**
     * Whether the user has folded the overlay away to the arrow, for the rest of the session.
     */
    public boolean perfOverlayCollapsed;

    public boolean perfOverlayClosePressed;

    public boolean perfOverlayArrowPressed;

    /**
     * The line of live numbers the overlay shows, already formatted by the app.
     */
    public String perfOverlayText = "";

    public final PanelState panelState = new PanelState();

    public final PopupState popupState = new PopupState();

    public void clone(final UiState uiState) {
        this.showTouchpad = uiState.showTouchpad;
        this.showPanel = uiState.showPanel;
        this.showPopup = uiState.showPopup;
        this.touchpadPressed = uiState.touchpadPressed;
        this.panelPressed = uiState.panelPressed;
        this.showContentNotStreamable = uiState.showContentNotStreamable;
        this.contentNotStreamableText = uiState.contentNotStreamableText;
        // performance overlay
        this.showPerfOverlay = uiState.showPerfOverlay;
        this.perfOverlayCollapsed = uiState.perfOverlayCollapsed;
        this.perfOverlayClosePressed = uiState.perfOverlayClosePressed;
        this.perfOverlayArrowPressed = uiState.perfOverlayArrowPressed;
        this.perfOverlayText = uiState.perfOverlayText;
        // panel state
        this.panelState.showMicButton = uiState.panelState.showMicButton;
        this.panelState.showFullscreenButton = uiState.panelState.showFullscreenButton;
        this.panelState.showAspectButton = uiState.panelState.showAspectButton;
        this.panelState.showVolumeButtons = uiState.panelState.showVolumeButtons;
        this.panelState.micButtonPressed = uiState.panelState.micButtonPressed;
        this.panelState.micButtonActive = uiState.panelState.micButtonActive;
        this.panelState.shareButtonPressed = uiState.panelState.shareButtonPressed;
        this.panelState.psButtonPressed = uiState.panelState.psButtonPressed;
        this.panelState.optionsButtonPressed = uiState.panelState.optionsButtonPressed;
        this.panelState.fullscreenButtonPressed = uiState.panelState.fullscreenButtonPressed;
        this.panelState.fullscreenButtonActive = uiState.panelState.fullscreenButtonActive;
        this.panelState.closeButtonPressed = uiState.panelState.closeButtonPressed;
        this.panelState.aspectButtonPressed = uiState.panelState.aspectButtonPressed;
        this.panelState.volumeDownPressed = uiState.panelState.volumeDownPressed;
        this.panelState.volumeUpPressed = uiState.panelState.volumeUpPressed;
        this.panelState.aspectModeIndex = uiState.panelState.aspectModeIndex;
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
    public boolean equals(final Object o) {
        if (this == o) {
            return true;
        }
        if (o == null || this.getClass() != o.getClass()) {
            return false;
        }
        final UiState uiState = (UiState) o;
        // no touchpadPressed and panelPressed as these are just internal config states. The overlay is
        // in here with its text and its pressed buttons, as a changed line has to reach the renderer
        return this.showTouchpad == uiState.showTouchpad && this.showPanel == uiState.showPanel && this.showPopup == uiState.showPopup && this.showContentNotStreamable == uiState.showContentNotStreamable && Objects.equals(this.contentNotStreamableText, uiState.contentNotStreamableText) && this.showPerfOverlay == uiState.showPerfOverlay && this.perfOverlayCollapsed == uiState.perfOverlayCollapsed && this.perfOverlayClosePressed == uiState.perfOverlayClosePressed && this.perfOverlayArrowPressed == uiState.perfOverlayArrowPressed && Objects.equals(this.perfOverlayText, uiState.perfOverlayText) && Objects.equals(this.panelState, uiState.panelState) && Objects.equals(this.popupState, uiState.popupState);
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.showTouchpad, this.showPanel, this.showPopup, this.showContentNotStreamable, this.showPerfOverlay, this.perfOverlayCollapsed, this.perfOverlayClosePressed, this.perfOverlayArrowPressed, this.perfOverlayText, this.panelState, this.popupState);
    }

    @Override
    public String toString() {
        return "UiState{" +
                "showTouchpad=" + this.showTouchpad +
                ", showPanel=" + this.showPanel +
                ", showPopup=" + this.showPopup +
                ", showContentNotStreamable=" + this.showContentNotStreamable +
                ", showPerfOverlay=" + this.showPerfOverlay +
                ", perfOverlayCollapsed=" + this.perfOverlayCollapsed +
                ", perfOverlayClosePressed=" + this.perfOverlayClosePressed +
                ", perfOverlayArrowPressed=" + this.perfOverlayArrowPressed +
                ", perfOverlayText='" + this.perfOverlayText + '\'' +
                ", panelState=" + this.panelState +
                ", popupState=" + this.popupState +
                '}';
    }

    /*********************/
    /*** inner classes ***/
    /*********************/

    public static class PanelState {
        public boolean showMicButton = true;
        public boolean showFullscreenButton = true;
        public boolean showAspectButton;

        /**
         * Whether the panel offers the pair of buttons that moves the volume of the output device of the
         * system. They take the slots right of the mic button, or its own when it is hidden.
         */
        public boolean showVolumeButtons;

        public boolean micButtonPressed;
        public boolean micButtonActive;
        public boolean shareButtonPressed;
        public boolean psButtonPressed;
        public boolean optionsButtonPressed;
        public boolean fullscreenButtonPressed;
        public boolean fullscreenButtonActive;
        public boolean closeButtonPressed;
        public boolean aspectButtonPressed;
        public boolean volumeDownPressed;
        public boolean volumeUpPressed;

        /**
         * Which video format the aspect ratio button shows, as the ordinal of the format enum of the
         * app. The renderer draws the icon that belongs to it.
         */
        public int aspectModeIndex;

        @Override
        public boolean equals(final Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || this.getClass() != o.getClass()) {
                return false;
            }
            final PanelState that = (PanelState) o;
            return this.showMicButton == that.showMicButton && this.showFullscreenButton == that.showFullscreenButton && this.showAspectButton == that.showAspectButton && this.showVolumeButtons == that.showVolumeButtons && this.micButtonPressed == that.micButtonPressed && this.micButtonActive == that.micButtonActive && this.shareButtonPressed == that.shareButtonPressed && this.psButtonPressed == that.psButtonPressed && this.optionsButtonPressed == that.optionsButtonPressed && this.fullscreenButtonPressed == that.fullscreenButtonPressed && this.fullscreenButtonActive == that.fullscreenButtonActive && this.closeButtonPressed == that.closeButtonPressed && this.aspectButtonPressed == that.aspectButtonPressed && this.volumeDownPressed == that.volumeDownPressed && this.volumeUpPressed == that.volumeUpPressed && this.aspectModeIndex == that.aspectModeIndex;
        }

        @Override
        public int hashCode() {
            return Objects.hash(this.showMicButton, this.showFullscreenButton, this.showAspectButton, this.showVolumeButtons, this.micButtonPressed, this.micButtonActive, this.shareButtonPressed, this.psButtonPressed, this.optionsButtonPressed, this.fullscreenButtonPressed, this.fullscreenButtonActive, this.closeButtonPressed, this.aspectButtonPressed, this.volumeDownPressed, this.volumeUpPressed, this.aspectModeIndex);
        }

        @Override
        public String toString() {
            return "PanelState{" +
                    "showMicButton=" + this.showMicButton +
                    ", showFullscreenButton=" + this.showFullscreenButton +
                    ", showAspectButton=" + this.showAspectButton +
                    ", showVolumeButtons=" + this.showVolumeButtons +
                    ", micButtonPressed=" + this.micButtonPressed +
                    ", micButtonActive=" + this.micButtonActive +
                    ", shareButtonPressed=" + this.shareButtonPressed +
                    ", psButtonPressed=" + this.psButtonPressed +
                    ", optionsButtonPressed=" + this.optionsButtonPressed +
                    ", fullscreenButtonPressed=" + this.fullscreenButtonPressed +
                    ", fullscreenButtonActive=" + this.fullscreenButtonActive +
                    ", closeButtonPressed=" + this.closeButtonPressed +
                    ", aspectButtonPressed=" + this.aspectButtonPressed +
                    ", volumeDownPressed=" + this.volumeDownPressed +
                    ", volumeUpPressed=" + this.volumeUpPressed +
                    ", aspectModeIndex=" + this.aspectModeIndex +
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
        public boolean equals(final Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || this.getClass() != o.getClass()) {
                return false;
            }
            final PopupState that = (PopupState) o;
            return this.showCheckbox == that.showCheckbox && this.checkboxChecked == that.checkboxChecked && this.checkboxFocused == that.checkboxFocused && this.leftButtonPressed == that.leftButtonPressed && this.leftButtonFocused == that.leftButtonFocused && this.rightButtonPressed == that.rightButtonPressed && this.rightButtonFocused == that.rightButtonFocused && Objects.equals(this.headerText, that.headerText) && Objects.equals(this.popupText, that.popupText) && Objects.equals(this.popupButtonLeft, that.popupButtonLeft) && Objects.equals(this.popupButtonRight, that.popupButtonRight) && Objects.equals(this.popupCheckboxText, that.popupCheckboxText);
        }

        @Override
        public int hashCode() {
            return Objects.hash(this.headerText, this.popupText, this.showCheckbox, this.popupButtonLeft, this.popupButtonRight, this.popupCheckboxText, this.checkboxChecked, this.checkboxFocused, this.leftButtonPressed, this.leftButtonFocused, this.rightButtonPressed, this.rightButtonFocused);
        }

        @Override
        public String toString() {
            return "PopupState{" +
                    "headerText='" + this.headerText + '\'' +
                    ", popupText='" + this.popupText + '\'' +
                    ", showCheckbox=" + this.showCheckbox +
                    ", popupButtonLeft='" + this.popupButtonLeft + '\'' +
                    ", popupButtonRight='" + this.popupButtonRight + '\'' +
                    ", popupCheckboxText='" + this.popupCheckboxText + '\'' +
                    ", checkboxChecked=" + this.checkboxChecked +
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
        ASPECT_BUTTON,
        VOLUME_DOWN_BUTTON,
        VOLUME_UP_BUTTON,
        FULLSCREEN_BUTTON,
        CLOSE_BUTTON,
        TOUCHPAD_BUTTON,
        PERF_CLOSE_BUTTON,
        PERF_RESTORE_BUTTON,
        DIALOG_BUTTON_LEFT,
        DIALOG_BUTTON_RIGHT,
        DIALOG_CHECKBOX_BUTTON

    }
}
