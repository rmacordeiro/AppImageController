# AppImage Controller
makes your Linux desktop AppImage ready™
<p align="center">
<img src="resources/icons/appimage-controller-icon.svg" alt="AppImage Controller logo"/>
</p>

This application is a clone of the AppImageLauncher developed by [TheAssassin](https://github.com/TheAssassin/AppImageLauncher).

Integrate AppImages to your application launcher with one click, and manage, update and remove them from there.
Double-click AppImages to open them, without having to make them executable first.

AppImage Controller plays well with other applications managing AppImages, for example app stores.
However, it doesn't depend on any of those, and can run completely standalone.

<p align="center">
<img src="https://github.com/rmacordeiro/AppImageController/raw/master/resources/doc/screenshot.png"/>
</p>

> ### :smiley: About AppImage Controller

AppImage Controller integrates AppImages with the system. It intercepts attempts to open an AppImage to provide its integration features.

AppImage Controller can control how the system treats AppImages. It integrates them into the system and provides helpers for updating or removing them.

When an AppImage is opened for the first time, AppImage Controller asks whether to integrate it or run it once. The dialog can remember that choice for the application, so later versions with the same embedded desktop identity follow the same preference.

The optional detection daemon records AppImages found in its watched directories and lists them in the **Detected AppImages** tab in AppImage Controller Settings. Detection does not integrate or otherwise modify discovered AppImages. Undecided items are marked as needing a decision; choose **Integrate** or **Do not integrate** in the list, or make a choice the next time the AppImage is opened. Preferences persist across sessions. Cross-version preferences require a single, unambiguous identity from embedded desktop metadata; otherwise, the choice applies only to that file path.

## Features

### AppImage desktop integration

This core feature allows you to integrate AppImages you download into your application menu or launcher, to make it easier for you to launch them. It also takes care of moving them into a central location, where you can find them later.
Furthermore, it sets up the update and removal entries in the launcher for you.

AppImages use the term "desktop integration", as they're not "installed" in the traditional sense. They remain single, self-contained executable files. AppImage Controller and other tools extract and patch the [desktop entry](https://specifications.freedesktop.org/desktop-entry-spec/latest/) as well as the related [icons](https://specifications.freedesktop.org/icon-theme-spec/icon-theme-spec-latest.html) into the relevant locations. Integrated files keep their original filename (without an added checksum). A trailing semantic version in the desktop entry's application name is omitted from the menu shortcut.
More information on desktop integration can be found in the [AppImage docs](https://docs.appimage.org/reference/desktop-integration.html).

### AppImage detection and remembered choices

When enabled, the detection daemon scans its configured watched directories and tracks AppImages it finds. Open **AppImage Controller Settings → Detected AppImages** to see the current list:

- **New - decision needed** means no integration preference has been saved for that AppImage.
- **Integrate** integrates the selected AppImage and saves that preference.
- **Do not integrate** saves a preference to run the AppImage without integrating it when opened.
- **Launch** runs the selected AppImage directly with its saved launch arguments. **Delete from disk** asks for confirmation, then removes the integration (if any), deletes the AppImage file and forgets it. If an AppImage already has a desktop entry that this tool did not create, editing its launch arguments shows a warning: that entry is left untouched and the arguments are only used by **Launch**. **Clear decision** removes a saved preference, so the app is undecided again.

The **Detected AppImages** table shows each app's name, saved action, file location, and **Launch Arguments**. The Location column is sized to its contents; if the complete paths do not fit on screen, use the horizontal scrollbar. Double-click the Launch Arguments cell to edit the optional arguments passed to the AppImage from its desktop entry (for example, `-f -v` or `--profile "Work Profile"`). Arguments are saved across sessions and, when embedded desktop metadata provides an unambiguous application identity, are reused across versions. For an already integrated AppImage, a valid edit regenerates its desktop entry and refreshes desktop caches immediately; if updating the integration fails, the previous arguments are restored. For an unintegrated AppImage, the saved arguments are applied when it is integrated. The Settings window opens at a larger default size and remembers its last size and position. It is scanned when Settings opens and only rescanned when **Refresh** is clicked; selecting another tab or taking an action does not scan watched folders. Opening an AppImage also records it. The table covers daemon-watched directories, not every location on the computer. Discovery itself never integrates, moves, or changes permissions on an AppImage. Opening an undecided AppImage uses the usual integrate-or-run-once dialog; checking **Remember my choice for this application** saves the choice for future sessions. The global setting to disable the prompt still applies to apps without a saved choice.

The **AppImage Controller** tab includes a language selector for the languages shipped with the project. The selection is saved and used by the Settings window and other AppImage Controller UI tools on their next start; **System default** follows the desktop locale.

In Settings, **Integrate** immediately checks whether integration is supported, moves the AppImage to the configured integration folder (or offers copying if moving fails), installs its desktop entry and icons, updates desktop caches, and saves the integrate preference after successful integration. If a same-named destination already exists, the user is asked whether to overwrite it. **Do not integrate** saves that preference and, if the AppImage is integrated, removes its desktop entry and system integration (the AppImage file itself is left in place). Integrating an AppImage that already has a desktop entry does not create another one: all application directories (user and system) are scanned for any `.desktop` file, from this tool or any other, whose `Exec`/`TryExec` launches that AppImage, and if one is found no new entry is created. Furthermore, an existing entry for the same AppImage (matched by recorded path, embedded application ID, or launch command, even under a different file name) never adds a "(1)" suffix to the menu name. Only the **Launch Arguments** column is editable; its text is appended to the desktop entry's launch command exactly as typed. The Settings window is a regular top-level window, so it can be minimized and maximized, and shows the application logo and version in the tab bar's top-right corner. **Clear decision** removes the saved preference so the app is undecided again; it does not undo an existing integration. The buttons update the displayed saved action without rescanning watched folders.

Preferences are matched using a single, unambiguous identity found in the AppImage's embedded desktop entry. If there is no reliable identity, the preference is associated only with the detected file path and cannot be carried over to another version or renamed file. Decisions remain saved when a file is removed from the detected list.

### Update management

After desktop integration, the context menu of the AppImage's entry in the application launcher will have an "Update" entry that launches a little helper tool to apply updates.

### Remove AppImages from system

Click the "Remove" entry in the context menu in the application launcher and the removal tool will ask you to confirm.
If you choose to do so, the desktop integration is undone, and the file is removed from your system.

### CLI

The packages ship with a CLI tool called `ail-cli`, providing basic operations in the terminal, for automation in scripts etc. As of February 2020, only integration and unintegration are supported. More features planned!

## About

Currently, desktop environments consider executables a security risk, and would rather have users use the app stores they include.

The ability to easily run them, however, isn't all that's needed to provide a good AppImage desktop experience. Making them accessible from the application menus and launchers is a level of "desktop integration" that can't be provided by the AppImages themselves properly (even though some AppImages ship with a "desktop integration script" prompting the user to do so). There are too many impliciations requiring external software, especially regarding cleanup and removal of AppImages. (If applications are simply made executable, they're still spread all over the users' personal files and folders.) The average user will not find a "Downloads" directory full of AppImages with cryptic filenames friendly.

Therefore, system-side ways have been developed to perform the desktop integration.
One of the first solutions was [appimaged](https://github.com/AppImage/appimaged), a daemon users could install to perform everything automagically in the background, without notifying the user in any way.
It scans a predefined set of directories including `~/Downloads` and `~/.bin`, making recognized AppImages executable and then performing the desktop integration. Those operations and monitoring produced a lot of file I/O, so were rather inefficent. Also, many users don't like the lack of control. The approach also opens attack vectors and thus can be considered a security hazard, as a vulnerability discovered in appimaged recently has shown.

**Traditional packages are highly recommended *if possible*, as they provide many more features and provide a much better overall experience.** The full build also produces an AppImage alongside the Debian and RPM packages; this AppImage is a portable build artifact, while the native packages provide the system integration required for the complete experience.

## Installation

### System-wide Installation

AppImage Controller integrates deeply into the system. Therefore, an installation via native system packages is the preferred way to install AppImage Controller. This way, AppImage Controller's package can perform the necessary steps to have your system use it for all AppImage invocations.

  - Ubuntu 24.04 and newer
  - Debian stable (Jessie, 8) and newer
  - Netrunner 17 and newer
  - openSUSE Leap 42 and newer
  - openSUSE Tumbleweed

The installation of packages on systems with a set of packages similar to one of the listed ones (e.g., Linux Mint, Fedora, etc.) should work as well.

Other systems derived from the listed ones, such as for instance Linux Mint (Ubuntu), should support AppImage Controller as well.

### Build from source

Build instructions in [BUILD.md](BUILD.md).

### Regenerating icons

`resources/icons/generate-icons.sh` renders the logo (`appimage-controller-icon.svg`) to `hicolor/` PNG sizes using Inkscape, or `rsvg-convert` if Inkscape is not installed. The logo SVG avoids masks and SVG2-only syntax so it also renders correctly in Qt.

## Background

This application is a clone of the AppImageLauncher developed by [TheAssassin](https://github.com/TheAssassin/AppImageLauncher).

## Support

If you enjoy this extension, consider buying me a tea 🍵 (I’m not really a coffee person :D)

<p align="center">
  <a href="https://www.buymeacoffee.com/rmacordeiro">
    <img src="https://github.com/user-attachments/assets/3b58a7fc-e605-4742-94e9-0bf3144c5021" width="200"/>
  </a>
</p>
