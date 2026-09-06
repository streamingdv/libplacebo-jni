# Replacing the library in PXPlay (LGPL)

This project is LGPL. PXPlay loads it as a normal classpath jar and unpacks the
matching native (`PlaceboManager.setupWithTemporaryFolder()`) at runtime, so a
user can rebuild this library and swap the jar in an installed copy without
rebuilding PXPlay. That is the intended way to satisfy the LGPL on the
Windows, Linux, and macOS ports.

The jar to replace is always **`libplacebo-jni-java-1.0.jar`**. Keep that
filename: the launcher class path refers to it by name.

## 1. Build the jar

Follow [Building and publishing the desktop jar](./README.md#building-and-publishing-the-desktop-jar):
drop the platform natives into `native-binaries/`, then:

```bash
./gradlew :libplacebo-jni-java:clean :libplacebo-jni-java:publishToMavenLocal
```

The artifact is
`~/.m2/repository/com/grill/placebo/libplacebo-jni-java/1.0/libplacebo-jni-java-1.0.jar`
(on Windows, under `%USERPROFILE%\.m2\...`). You can also take
`libplacebo-jni-java/build/libs/libplacebo-jni-java-1.0.jar` after
`:libplacebo-jni-java:jar`.

PXPlay itself consumes the same coordinate from `mavenLocal()`
(`com.grill.placebo:libplacebo-jni-java:1.0`). Rebuilding PXPlay after
`publishToMavenLocal` is optional; replacing the installed jar is enough.

## 2. Drop it into the install

Quit PXPlay first. Overwrite the existing file (admin / `sudo` on a system
install).

| Port | `libs` folder |
| --- | --- |
| **Windows** | `C:\Program Files\PXPlay\libs\` (or `<PXPlay>\libs\` for a portable copy) |
| **Linux** | `/opt/PXPlay/libs/` (or `<PXPlay>/libs/` for a tarball / unpacked build) |
| **macOS** | `/Applications/PXPlay.app/Contents/Resources/Java/libs/` |

On macOS, changing a file inside the bundle invalidates the notarized
signature. Ad-hoc re-sign so Gatekeeper will still launch it:

```bash
codesign --force --deep --sign - /Applications/PXPlay.app
xattr -cr /Applications/PXPlay.app
```

The next start unpacks natives from the new jar into a temp directory; no extra
native files need to be copied next to the exe.
