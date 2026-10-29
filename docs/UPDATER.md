# Console updater (0.31.1)

The RareStage Console updates itself the way RareStudio does. It never changes anything on its own while a show is
running: it checks for a release, offers it, and installs it only when the operator presses **Install update**. The new
version is used the next time the console starts.

## What the operator sees

* On start the console checks in the background (switch it off under **Output > Updates**). A banner appears at the top:
  *UPDATE AVAILABLE 0.31.1 -> 0.32.0* with **Install update** and **Skip this version**. A skipped release is not offered
  by the automatic check again; **Check for updates** offers it again.
* **Install update** downloads the release (progress bar), verifies it, unpacks it and says *0.32.0 is installed. Restart
  the console to use it.* **Restart console now** opens the new version.
* The renderer (`RareStage.exe`) is never closed or replaced while it runs. After a restart the new console tells you to
  close the old renderer and launch it again from the console, because console and renderer must be the same version.
* **Output > Updates** lists the older versions kept on the computer (the newest two) with **Go back to x.y.z**.

## How it works

* **Feed.** `https://github.com/rare6media/RareStage/releases/latest/download/latest.json`:

  ```json
  { "version": "0.32.0", "url": "https://github.com/.../RareStage-0.32.0-windows-x64.zip", "sha256": "<64 hex>", "notes": "..." }
  ```

  The feed must carry a valid version, an https download address and a SHA-256. A feed without a checksum, or with an
  `http://` address, is refused.
* **Verification.** The zip is downloaded over https (GitHub's redirect to its CDN is followed, https to http is not),
  its SHA-256 must equal the feed's, it is unpacked with the Windows `tar.exe`, and the result must contain
  `RareStage.exe` and `RareStageConsole.exe`. Any failure discards the download and installs nothing.
* **Layout.** Versions are unpacked beside each other in `%LOCALAPPDATA%\RareStage\updates\<version>\`, so nothing that is
  installed is overwritten. An `ok` file is written last; a folder without it is never used. A zip with a single top-level
  folder (the CI package) and a flat zip both work.
* **Launcher.** The console is its own launcher: on start it hands over to the newest folder that has the `ok` marker and
  both programs. The shortcut never changes. `--no-update-launch` runs the copy you started instead, whatever is newer.
* **Rollback.** *Go back to x.y.z* removes every newer version after the console exits and starts the older one; the version
  you left is remembered as skipped so it is not downloaded again straight away.
* **Settings** are in `%LOCALAPPDATA%\RareStage\updater.ini` (`Auto`, `Skip`, `Keep`, and `Url` to override the feed with
  another https address or, for testing, a local file). They are kept apart from `settings.ini`, which the renderer rewrites.

## Publishing a release

`package-release.ps1` builds the package folder and, in `..\RareStage-release-<version>\`, the zip, its checksum and
`latest.json` (with the GitHub release download address for tag `v<version>`). Upload all three to the GitHub release.
The CI workflow does the same for pushes to `main`. **A release without `latest.json` is invisible to the updater.**

## Limits

* Versions older than the updater (0.31.0 and earlier) cannot update themselves; install 0.31.1 or later once by hand.
* The Classic console and a renderer started directly (not through the console) are not updated by this.
* Downloads are not signed, only checksummed against the feed, which is itself served by GitHub over https. If the
  repository or release is compromised, so is the update. Authenticode signing is not set up.
* Update installs were exercised over the real network with a feed announcing a newer version and the actual v0.31.0
  release zip; a genuine newer GitHub release has not yet been through the updater.
