/**
 * Puts text that reads right to left into the order nuklear draws it in.
 *
 * nuklear walks the bytes of a string and draws one glyph after the other from left to right,
 * which turns a hebrew word into its own mirror image. What sorts that out is the bidirectional
 * algorithm of unicode, and this is the part of it a user interface needs: one line at a time,
 * no explicit embedding codes, hebrew and arabic as the right to left scripts, and the numbers
 * and latin words inside a hebrew sentence kept reading in their own direction.
 *
 * Only the order of the glyphs changes here. The app does not mirror its layout for hebrew
 * either, JavaFX reorders the characters of a label and leaves everything else alone, so the
 * overlay keeps its buttons and its alignment where every other language has them. That also
 * keeps the metrics the java side hit tests untouched, see dialog_ui.h.
 *
 * Copies of this file have to stay identical:
 *
 *   - streamutils:            libstreamutils-native/src/nuklear/bidi_text.h
 *   - libplacebo-jni:         libplacebo-jni-native/src/bidi_text.h
 *
 * nuklear.h has to be included before this file.
 */
#ifndef PXPLAY_BIDI_TEXT_H
#define PXPLAY_BIDI_TEXT_H

/* The longest line that is reordered. Anything longer is drawn the way it was handed over, which
 * is what happened to all of it before: no line of the overlay comes near this. */
#define NK_BIDI_MAX_RUNES 512
/* The buffer a caller has to bring: every rune of the line as utf-8, and the terminator. */
#define NK_BIDI_MAX_BYTES ((NK_BIDI_MAX_RUNES * 4) + 1)

/* The character types the algorithm needs, which is a subset of the bidirectional classes. */
#define NK_BIDI_L  0    /* a letter that reads left to right: latin, cyrillic, cjk */
#define NK_BIDI_R  1    /* a letter that reads right to left: hebrew, arabic */
#define NK_BIDI_EN 2    /* a digit */
#define NK_BIDI_ES 3    /* the sign of a number: + - */
#define NK_BIDI_CS 4    /* what separates the parts of one: , . : / */
#define NK_BIDI_ET 5    /* what a number is written with: # $ % and the currencies */
#define NK_BIDI_WS 6    /* a space */
#define NK_BIDI_ON 7    /* neutral: punctuation, brackets, symbols */

/**
 * The bidirectional class of the character.
 *
 * Whole blocks are taken for the right to left scripts and the punctuation, and everything that
 * is left over reads left to right. That is coarser than the character database of unicode:
 * a handful of latin-1 symbols come out neutral although they are letters, and a combining mark
 * takes the class of its block instead of the one of the letter it sits on. Neither can be seen
 * in a line of the overlay, whose text is hebrew, latin, digits and punctuation.
 */
static int nk_bidi_class_of(nk_rune rune)
{
    /* hebrew, arabic, syriac, thaana and the rest of the right to left blocks, their presentation
     * forms, and the mark that turns the direction of what follows */
    if ((rune >= 0x0590 && rune <= 0x08FF) || (rune >= 0xFB1D && rune <= 0xFDFF)
        || (rune >= 0xFE70 && rune <= 0xFEFC) || rune == 0x200F || rune == 0x061C) {
        return NK_BIDI_R;
    }
    if (rune >= '0' && rune <= '9') return NK_BIDI_EN;
    if (rune == 0x200E) return NK_BIDI_L;    /* the left to right mark */
    if (rune == ' ' || rune == '\t' || rune == '\n' || rune == '\r' || rune == 0x00A0
        || (rune >= 0x2000 && rune <= 0x200A) || rune == 0x3000) {
        return NK_BIDI_WS;
    }
    if (rune == '+' || rune == '-' || rune == 0x2212) return NK_BIDI_ES;
    if (rune == ',' || rune == '.' || rune == ':' || rune == '/') return NK_BIDI_CS;
    if (rune == '#' || rune == '$' || rune == '%' || rune == 0x00B0 || rune == 0x00B1
        || (rune >= 0x00A2 && rune <= 0x00A5) || rune == 0x2030 || rune == 0x2031
        || (rune >= 0x20A0 && rune <= 0x20CF)) {
        return NK_BIDI_ET;
    }
    if (rune <= 0x0040) return NK_BIDI_ON;                       /* the ascii below the letters */
    if (rune >= 0x005B && rune <= 0x0060) return NK_BIDI_ON;
    if (rune >= 0x007B && rune <= 0x00BF) return NK_BIDI_ON;     /* the latin-1 punctuation */
    if (rune == 0x00D7 || rune == 0x00F7) return NK_BIDI_ON;     /* the times and divide signs */
    if (rune >= 0x2010 && rune <= 0x206F) return NK_BIDI_ON;     /* the general punctuation */
    return NK_BIDI_L;
}

/** Which side a resolved character counts as, where a digit counts as right to left. */
static int nk_bidi_side_of(int characterClass)
{
    return (characterClass == NK_BIDI_R || characterClass == NK_BIDI_EN) ? NK_BIDI_R : NK_BIDI_L;
}

/** The character that opens what this one closes, for the brackets of a right to left run. */
static nk_rune nk_bidi_mirror(nk_rune rune)
{
    switch (rune) {
    case '(': return ')';
    case ')': return '(';
    case '[': return ']';
    case ']': return '[';
    case '{': return '}';
    case '}': return '{';
    case '<': return '>';
    case '>': return '<';
    case 0x00AB: return 0x00BB;
    case 0x00BB: return 0x00AB;
    case 0x2039: return 0x203A;
    case 0x203A: return 0x2039;
    default: return rune;
    }
}

/** Whether the text holds a right to left letter, which is the only reason to reorder anything. */
static nk_bool nk_bidi_has_rtl(const char* text, int length)
{
    int at = 0;

    if (!text) return nk_false;

    while (at < length) {
        nk_rune rune = 0;
        int runeLength = nk_utf_decode(&text[at], &rune, length - at);
        if (runeLength < 1) return nk_false;
        if (nk_bidi_class_of(rune) == NK_BIDI_R) return nk_true;
        at += runeLength;
    }
    return nk_false;
}

/**
 * Writes the line in the order it reads on the screen and returns how many bytes that took, or
 * zero when there is nothing to reorder, the line is longer than what fits, or its utf-8 is not
 * something nuklear can decode. A caller that gets zero has to draw the text it started with.
 *
 * The buffer is terminated, so it can be handed to whatever wants a string of its own.
 */
static int nk_bidi_reorder(const char* text, int length, char* out, int capacity)
{
    nk_rune runes[NK_BIDI_MAX_RUNES];
    unsigned char classes[NK_BIDI_MAX_RUNES];
    unsigned char levels[NK_BIDI_MAX_RUNES];
    int order[NK_BIDI_MAX_RUNES];
    int count = 0;
    int at = 0;
    int i;
    int written = 0;
    int base = NK_BIDI_L;
    int strong;
    int highest = 0;
    int lowestOdd = 0;
    int level;

    if (!text || length < 1 || !out || capacity < 2) return 0;

    /* the runes of the line and what each of them is */
    while (at < length) {
        nk_rune rune = 0;
        int runeLength = nk_utf_decode(&text[at], &rune, length - at);
        if (runeLength < 1 || rune == NK_UTF_INVALID || count >= NK_BIDI_MAX_RUNES) return 0;
        runes[count] = rune;
        classes[count] = (unsigned char) nk_bidi_class_of(rune);
        at += runeLength;
        count++;
    }
    if (count < 1) return 0;

    /* the direction of the line, which is the one of the first letter that has one */
    for (i = 0; i < count; ++i) {
        if (classes[i] == NK_BIDI_R || classes[i] == NK_BIDI_L) {
            base = classes[i];
            break;
        }
    }

    /* a sign or a separator between two digits belongs to the number, so 1,5 and 12:30 stay whole */
    for (i = 1; i < count - 1; ++i) {
        if ((classes[i] == NK_BIDI_ES || classes[i] == NK_BIDI_CS)
            && classes[i - 1] == NK_BIDI_EN && classes[i + 1] == NK_BIDI_EN) {
            classes[i] = NK_BIDI_EN;
        }
    }
    /* what a number is written with joins it as well, which is what keeps the %1$s of a translated
     * line reading the way it was written instead of coming out as s$1% */
    for (i = 0; i < count; ++i) {
        int runStart;
        int runEnd;
        int j;

        if (classes[i] != NK_BIDI_ET) continue;

        runStart = i;
        while (i < count && classes[i] == NK_BIDI_ET) i++;
        runEnd = i;
        i--;

        if ((runStart > 0 && classes[runStart - 1] == NK_BIDI_EN)
            || (runEnd < count && classes[runEnd] == NK_BIDI_EN)) {
            for (j = runStart; j < runEnd; ++j) classes[j] = NK_BIDI_EN;
        }
    }
    /* every other sign and separator is left to the text around it */
    for (i = 0; i < count; ++i) {
        if (classes[i] == NK_BIDI_ES || classes[i] == NK_BIDI_CS || classes[i] == NK_BIDI_ET) {
            classes[i] = NK_BIDI_ON;
        }
    }
    /* a number behind a latin word belongs to that word, which keeps the 5 of a PS5 out of the
     * hebrew run that follows it */
    strong = base;
    for (i = 0; i < count; ++i) {
        if (classes[i] == NK_BIDI_R || classes[i] == NK_BIDI_L) strong = classes[i];
        else if (classes[i] == NK_BIDI_EN && strong == NK_BIDI_L) classes[i] = NK_BIDI_L;
    }

    /* what stands between two runs of the same direction joins them, everything else neutral
     * follows the direction of the line */
    for (i = 0; i < count; ++i) {
        int runStart;
        int runEnd;
        int before;
        int behind;
        int direction;
        int j;

        if (classes[i] != NK_BIDI_WS && classes[i] != NK_BIDI_ON) continue;

        runStart = i;
        while (i < count && (classes[i] == NK_BIDI_WS || classes[i] == NK_BIDI_ON)) i++;
        runEnd = i;
        i--;    /* the loop steps over the character behind the run on its own */

        before = (runStart > 0) ? nk_bidi_side_of(classes[runStart - 1]) : base;
        behind = (runEnd < count) ? nk_bidi_side_of(classes[runEnd]) : base;
        direction = (before == behind) ? before : base;
        for (j = runStart; j < runEnd; ++j) classes[j] = (unsigned char) direction;
    }

    /* the depth every character is drawn at: odd reads right to left, even left to right. A digit
     * or a latin word inside hebrew goes one deeper, which is what keeps it readable. */
    for (i = 0; i < count; ++i) {
        if (classes[i] == NK_BIDI_R) levels[i] = 1;
        else if (base == NK_BIDI_R) levels[i] = 2;
        else levels[i] = (classes[i] == NK_BIDI_EN) ? 2 : 0;
    }

    /* the spaces at the end follow the line and not the letter in front of them, so a hebrew line
     * does not begin with the blanks of its own end */
    for (i = count - 1; i >= 0; --i) {
        if (nk_bidi_class_of(runes[i]) != NK_BIDI_WS) break;
        levels[i] = (unsigned char) ((base == NK_BIDI_R) ? 1 : 0);
    }

    for (i = 0; i < count; ++i) {
        order[i] = i;
        if (levels[i] > highest) highest = levels[i];
        if ((levels[i] & 1) && (lowestOdd == 0 || levels[i] < lowestOdd)) lowestOdd = levels[i];
    }
    if (lowestOdd == 0) return 0;    /* nothing reads right to left after all */

    /* the reordering itself: every run of the deepest level is turned around, then every run of
     * the level above it, up to the shallowest one that reads right to left */
    for (level = highest; level >= lowestOdd; --level) {
        int start = 0;
        while (start < count) {
            int end;
            int front;
            int back;

            if (levels[start] < level) {
                start++;
                continue;
            }
            end = start;
            while (end < count && levels[end] >= level) end++;

            front = start;
            back = end - 1;
            while (front < back) {
                int swapped = order[front];
                order[front] = order[back];
                order[back] = swapped;
                front++;
                back--;
            }
            start = end;
        }
    }

    for (i = 0; i < count; ++i) {
        const int index = order[i];
        const nk_rune rune = (levels[index] & 1) ? nk_bidi_mirror(runes[index]) : runes[index];
        const int runeLength = nk_utf_encode(rune, &out[written], (capacity - 1) - written);
        if (runeLength < 1) return 0;
        written += runeLength;
    }
    out[written] = '\0';
    return written;
}

/**
 * The text as it has to be drawn: the buffer when there was something to reorder, the text itself
 * when there was not. visualLength takes the byte count to draw, which is optional.
 */
static const char* nk_bidi_visual(const char* text, int length, char* buffer, int capacity,
                                  int* visualLength)
{
    int written;

    if (visualLength) *visualLength = length;
    if (!text || length < 1 || !buffer || capacity < 2) return text;
    if (!nk_bidi_has_rtl(text, length)) return text;

    written = nk_bidi_reorder(text, length, buffer, capacity);
    if (written < 1) return text;

    if (visualLength) *visualLength = written;
    return buffer;
}

#endif /* PXPLAY_BIDI_TEXT_H */
