# RareStage 0.31.1

## What is new

* **Console updater.** The RareStage Console now checks GitHub for a newer release when it starts and offers it in a banner.
  Installing is always the operator's choice, the download is verified against a SHA-256 in the release feed, new
  versions are unpacked beside the old ones, and the renderer is never touched while it runs. Older versions are kept
  for **Go back to...** under Output > Updates. See `docs/UPDATER.md`.
* The release package no longer includes the log and adapter files from the machine that built it.

## Upgrade notes

* 0.31.0 and earlier have no updater; install 0.31.1 by hand once. After that the console updates itself.
* The control protocol is still **30**. After an update the renderer must be closed and launched again from the new
  console so both are the same version.
* New console switches: `--no-update-launch` (run this copy even if a newer version is installed).
* New file: `%LOCALAPPDATA%\RareStage\updater.ini`. Updates are unpacked under `%LOCALAPPDATA%\RareStage\updates\`.

## Verification

Console updater tests (`RareStageUpdaterTests`, no network needed): versions, feed parsing and refusal of unsafe feeds,
SHA-256, folder handling, cleaning, and the whole check, install, skip and failure cycle with real zips (wrong checksum,
corrupt zip, zip without the program, missing and insecure feeds). Over the real network: a feed announcing a newer
version with the actual v0.31.0 release zip as its download downloaded over https, matched its checksum, unpacked, and
the unpacked console and renderer passed their startup and audio self-tests. All earlier suites still pass.

## Known issues and things not verified

* No genuine newer GitHub release has gone through the updater yet; that happens with the first release after this one.
* Downloads are checksummed, not signed.
* The 0.31.0 release zip includes a startup log and adapter file left over from the build machine.
* Everything listed in `docs/ROADMAP.md` as in progress or not started still applies.
