# RareStage security and release verification

Download RareStage only from the official `rare6media/RareStage` GitHub Releases page. Each v0.10.5-or-newer release includes `RareStage-SHA256SUMS.txt`; compare its SHA-256 values with the downloaded ZIP or extracted executables before running them.

RareStage and RareStage Console are native Windows executables. Console launches the adjacent `RareStage.exe` through the standard Windows shell and communicates with it through a versioned local shared-memory snapshot and acknowledged window messages. The application has no updater, downloader, service, driver, autorun entry, process injection, PowerShell invocation or command-shell invocation.

## Production-clock network boundary

RareStage v0.12 adds an optional UDP production clock on port 47712. Local mode is the default and opens no network socket. Leader mode broadcasts a fixed-size state packet; Follower mode accepts only packets with the RareStage clock magic, exact protocol version and packet size, and matching numeric sync group. Packets contain show timing and visual-state numbers only. They cannot request code execution, filesystem access, downloads, process launches or Console commands.

Sync groups reduce accidental cross-show coupling but are not authentication or encryption. Use Leader/Follower mode only on a trusted, isolated production LAN or VLAN, restrict Windows Firewall scope to the venue's private subnet, arm one Leader per group, and never expose UDP 47712 directly to the public internet. A Follower that stops receiving valid packets enters hold: it preserves the last accepted visual state while its local continuous animation keeps running.

The release package contains `RareStage-build-provenance.json`, recording the source commit, workflow run, exact pinned Spout2 revision and Authenticode signing status. Official builds are compiled and tested by the public Windows GitHub Actions workflow. Authenticode signing is applied only when the repository's protected signing certificate is configured; the provenance field is authoritative and the documentation does not claim an unsigned build is signed.

## Antivirus reports

Machine-learning antivirus products can occasionally classify a new, low-prevalence unsigned executable as suspicious. Do not disable antivirus or add an exclusion merely to run RareStage. First verify the release source and SHA-256 value. If the hash matches but Windows Security still reports the file, quarantine it and submit the official ZIP and detection details to Microsoft for false-positive review. A matching hash establishes package identity, not that an antivirus verdict is wrong.

## Reporting a vulnerability

Open a private security advisory in the GitHub repository with the affected version, reproduction steps and impact. Do not include credentials, private show files or unrelated personal data. Public issues are appropriate for ordinary crashes and renderer defects that do not expose a security boundary.
