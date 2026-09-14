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

    /**
     * Whether the hint naming the button a player joins the session with is on screen. Shown a few
     * seconds at a time, and only by a session that lets more than one player in.
     */
    public boolean showJoinHint;

    /**
     * The sentence of that hint, already translated by the app.
     */
    public String joinHintText = "";

    /**
     * How many player seats the controller overlay shows, counting the empty ones below the highest one
     * the session has handed out, or zero for a session that shows no overlay at all.
     */
    public int padOverlaySeats;

    /**
     * Bit per seat of that overlay, set while a controller is holding it.
     */
    public int padOverlayConnectedMask;

    /**
     * Bit per seat of that overlay, set once the console has let that player into the session, which is
     * what tells a controller the game is listening to from one whose player still has to ask.
     */
    public int padOverlayJoinedMask;

    /**
     * What the seats' controllers are called, newline separated and in seat order, already what the app
     * wants shown for one it had nothing better to call.
     */
    public String padOverlayNames = "";

    /**
     * Whether that overlay is to be drawn with the button panel down, the app holding it up for a moment
     * because the seating has just changed. Otherwise it comes and goes with the panel.
     */
    public boolean padOverlayHold;

    /**
     * Whether the card a joining player picks the account they join with on is up. It dims the stream and
     * takes the input over the way a dialog does, and only a session on a PlayStation 5 that lets more
     * than one player in ever shows one.
     */
    public boolean showPlayerPicker;

    public final PanelState panelState = new PanelState();

    public final PopupState popupState = new PopupState();

    public final PlayerPickerState playerPickerState = new PlayerPickerState();

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
        // join hint
        this.showJoinHint = uiState.showJoinHint;
        this.joinHintText = uiState.joinHintText;
        // controller overlay
        this.padOverlaySeats = uiState.padOverlaySeats;
        this.padOverlayConnectedMask = uiState.padOverlayConnectedMask;
        this.padOverlayJoinedMask = uiState.padOverlayJoinedMask;
        this.padOverlayNames = uiState.padOverlayNames;
        this.padOverlayHold = uiState.padOverlayHold;
        // the account picker
        this.showPlayerPicker = uiState.showPlayerPicker;
        this.playerPickerState.page = uiState.playerPickerState.page;
        this.playerPickerState.focused = uiState.playerPickerState.focused;
        this.playerPickerState.accountCount = uiState.playerPickerState.accountCount;
        this.playerPickerState.title = uiState.playerPickerState.title;
        this.playerPickerState.message = uiState.playerPickerState.message;
        this.playerPickerState.hint = uiState.playerPickerState.hint;
        this.playerPickerState.accountNames = uiState.playerPickerState.accountNames;
        this.playerPickerState.monograms = uiState.playerPickerState.monograms;
        this.playerPickerState.newAccountText = uiState.playerPickerState.newAccountText;
        this.playerPickerState.cancelText = uiState.playerPickerState.cancelText;
        this.playerPickerState.backText = uiState.playerPickerState.backText;
        this.playerPickerState.showBackButton = uiState.playerPickerState.showBackButton;
        this.playerPickerState.cancelPressed = uiState.playerPickerState.cancelPressed;
        this.playerPickerState.backPressed = uiState.playerPickerState.backPressed;
        this.playerPickerState.qrModuleCount = uiState.playerPickerState.qrModuleCount;
        this.playerPickerState.qrModules = uiState.playerPickerState.qrModules;
        this.playerPickerState.typedDigits = uiState.playerPickerState.typedDigits;
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
        return this.showTouchpad == uiState.showTouchpad && this.showPanel == uiState.showPanel && this.showPopup == uiState.showPopup && this.showContentNotStreamable == uiState.showContentNotStreamable && Objects.equals(this.contentNotStreamableText, uiState.contentNotStreamableText) && this.showPerfOverlay == uiState.showPerfOverlay && this.perfOverlayCollapsed == uiState.perfOverlayCollapsed && this.perfOverlayClosePressed == uiState.perfOverlayClosePressed && this.perfOverlayArrowPressed == uiState.perfOverlayArrowPressed && Objects.equals(this.perfOverlayText, uiState.perfOverlayText) && this.showJoinHint == uiState.showJoinHint && Objects.equals(this.joinHintText, uiState.joinHintText) && this.padOverlaySeats == uiState.padOverlaySeats && this.padOverlayConnectedMask == uiState.padOverlayConnectedMask && this.padOverlayJoinedMask == uiState.padOverlayJoinedMask && Objects.equals(this.padOverlayNames, uiState.padOverlayNames) && this.padOverlayHold == uiState.padOverlayHold && this.showPlayerPicker == uiState.showPlayerPicker && Objects.equals(this.panelState, uiState.panelState) && Objects.equals(this.popupState, uiState.popupState) && Objects.equals(this.playerPickerState, uiState.playerPickerState);
    }

    @Override
    public int hashCode() {
        return Objects.hash(this.showTouchpad, this.showPanel, this.showPopup, this.showContentNotStreamable, this.showPerfOverlay, this.perfOverlayCollapsed, this.perfOverlayClosePressed, this.perfOverlayArrowPressed, this.perfOverlayText, this.showJoinHint, this.joinHintText, this.padOverlaySeats, this.padOverlayConnectedMask, this.padOverlayJoinedMask, this.padOverlayNames, this.padOverlayHold, this.showPlayerPicker, this.panelState, this.popupState, this.playerPickerState);
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
                ", showJoinHint=" + this.showJoinHint +
                ", joinHintText='" + this.joinHintText + '\'' +
                ", padOverlaySeats=" + this.padOverlaySeats +
                ", padOverlayConnectedMask=" + this.padOverlayConnectedMask +
                ", padOverlayJoinedMask=" + this.padOverlayJoinedMask +
                ", padOverlayNames='" + this.padOverlayNames + '\'' +
                ", padOverlayHold=" + this.padOverlayHold +
                ", showPlayerPicker=" + this.showPlayerPicker +
                ", panelState=" + this.panelState +
                ", popupState=" + this.popupState +
                ", playerPickerState=" + this.playerPickerState +
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

    /**
     * The card a joining player picks the account they join with on, in the state the app last pushed.
     * <p>
     * Which tile carries the highlight is the app's answer and not the renderer's: the controller that
     * asked to join is what moves it, and the mouse moves it as well, so a renderer only ever draws the
     * highlight where it was put.
     */
    public static class PlayerPickerState {

        /** The accounts to pick from. */
        public static final int PAGE_ACCOUNTS = 0;

        /** The code a phone reads to sign a new account in. */
        public static final int PAGE_QR_CODE = 1;

        /** The wait while that login is being finished. */
        public static final int PAGE_SIGNING_IN = 2;

        /** The passcode of the account being joined with, which the console asks for where it is protected. */
        public static final int PAGE_PASSCODE = 3;

        public int page = PlayerPickerState.PAGE_ACCOUNTS;

        /**
         * Which tile carries the highlight, counting from the leftmost account. The tile that signs a new
         * account in comes after the last account, so {@link #accountCount} is its own index.
         */
        public int focused;

        /** How many accounts are on offer, which the tile signing a new one in is drawn after. */
        public int accountCount;

        public String title = "";

        /** The line above the code, shown on that page only. */
        public String message = "";

        /** The line below the tiles or the code, saying the console has to hold the account as a user. */
        public String hint = "";

        /** What the accounts are called, newline separated and in tile order. */
        public String accountNames = "";

        /**
         * The letters drawn in the circles of those accounts, newline separated and in the same order, one
         * line per account. The app takes them from the names, as it is the one that knows how a name of
         * its language begins.
         */
        public String monograms = "";

        public String newAccountText = "";
        public String cancelText = "";
        public String backText = "";

        /** Whether the card offers the way back to the accounts, which the code page does. */
        public boolean showBackButton;

        public boolean cancelPressed;
        public boolean backPressed;

        /**
         * How many modules the code has along each edge, quiet zone included, or zero while the card shows
         * no code.
         */
        public int qrModuleCount;

        /**
         * One byte per module of that code, row by row and non zero for a dark one. Set once per code and
         * never written into afterwards, which is why two states that hold the same array are the same
         * card to {@link #equals(Object)}.
         */
        public byte[] qrModules;

        /**
         * How many digits of the passcode have been typed, on that page only, which is how many of its boxes
         * are filled and which one of them is lit.
         * <p>
         * A count and never the digits: what a player types is an answer to their own console, and neither
         * this state nor the renderer reading it has any reason to be told it.
         */
        public int typedDigits;

        @Override
        public boolean equals(final Object o) {
            if (this == o) {
                return true;
            }
            if (o == null || this.getClass() != o.getClass()) {
                return false;
            }
            final PlayerPickerState that = (PlayerPickerState) o;
            return this.page == that.page && this.focused == that.focused && this.accountCount == that.accountCount && this.showBackButton == that.showBackButton && this.cancelPressed == that.cancelPressed && this.backPressed == that.backPressed && this.qrModuleCount == that.qrModuleCount && this.qrModules == that.qrModules && this.typedDigits == that.typedDigits && Objects.equals(this.title, that.title) && Objects.equals(this.message, that.message) && Objects.equals(this.hint, that.hint) && Objects.equals(this.accountNames, that.accountNames) && Objects.equals(this.monograms, that.monograms) && Objects.equals(this.newAccountText, that.newAccountText) && Objects.equals(this.cancelText, that.cancelText) && Objects.equals(this.backText, that.backText);
        }

        @Override
        public int hashCode() {
            // the modules themselves are left out, two codes of the same size hashing alike being cheaper
            // than walking a few thousand bytes of one
            return Objects.hash(this.page, this.focused, this.accountCount, this.title, this.message, this.hint, this.accountNames, this.monograms, this.newAccountText, this.cancelText, this.backText, this.showBackButton, this.cancelPressed, this.backPressed, this.qrModuleCount, this.typedDigits);
        }

        @Override
        public String toString() {
            return "PlayerPickerState{" +
                    "page=" + this.page +
                    ", focused=" + this.focused +
                    ", accountCount=" + this.accountCount +
                    ", title='" + this.title + '\'' +
                    ", message='" + this.message + '\'' +
                    ", hint='" + this.hint + '\'' +
                    ", accountNames='" + this.accountNames + '\'' +
                    ", monograms='" + this.monograms + '\'' +
                    ", newAccountText='" + this.newAccountText + '\'' +
                    ", cancelText='" + this.cancelText + '\'' +
                    ", backText='" + this.backText + '\'' +
                    ", showBackButton=" + this.showBackButton +
                    ", cancelPressed=" + this.cancelPressed +
                    ", backPressed=" + this.backPressed +
                    ", qrModuleCount=" + this.qrModuleCount +
                    ", typedDigits=" + this.typedDigits +
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
        DIALOG_CHECKBOX_BUTTON,

        /** Lets go of the join the card of the account picker was asked for. */
        PLAYER_PICKER_CANCEL_BUTTON,

        /** Goes back from the code a phone reads to the accounts. */
        PLAYER_PICKER_BACK_BUTTON

    }
}
