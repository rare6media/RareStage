#pragma once
// Self-updater for the RareStage Console, modelled on the RareStudio updater so the Rare apps behave the same way.
//
//  - Feed: a small JSON file, by default the latest GitHub release's `latest.json` asset:
//      { "version": "0.32.0", "url": "https://.../RareStage-windows-x64.zip", "sha256": "<64 hex>", "notes": "optional" }
//  - A new version is only ever downloaded when the operator asks for it. The zip must match the feed's SHA-256,
//    is unpacked beside the old versions under %LOCALAPPDATA%\RareStage\updates\<version>\, and is marked usable with an
//    "ok" file written last. Nothing is overwritten and the renderer is never touched while it is running.
//  - The console acts as its own launcher: on start it hands over to the newest usable version, so the shortcut never
//    changes. Older versions stay for rollback; --no-update-launch starts the copy you launched, whatever is newer.
//  - Settings live in %LOCALAPPDATA%\RareStage\updater.ini (not settings.ini, which the renderer rewrites).
//
// Network: WinHTTP, https only. A local feed (a file path in updater.ini, "Url=") is allowed for testing.
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace rarestage::update {

namespace fs = std::filesystem;

inline constexpr const char* DefaultFeed = "https://github.com/rare6media/RareStage/releases/latest/download/latest.json";
inline constexpr size_t MaxFeedBytes = 256 * 1024;
inline constexpr std::uintmax_t MaxZipBytes = 400ull * 1024 * 1024;

struct Feed { std::string version, url, sha256, notes; };

// ---- versions ---------------------------------------------------------------------------------
// "0.31.0", "v0.31.1" and "0.31" are accepted; anything else is not a version.
inline bool ParseVersion(const std::string& text, std::vector<int>& parts) {
    parts.clear();
    size_t i = (!text.empty() && (text[0] == 'v' || text[0] == 'V')) ? 1 : 0;
    long current = -1;
    for (; i <= text.size(); ++i) {
        const char c = i < text.size() ? text[i] : '.';
        if (c >= '0' && c <= '9') { current = (current < 0 ? 0 : current) * 10 + (c - '0'); if (current > 100000) return false; }
        else if (c == '.' && current >= 0) { parts.push_back(int(current)); current = -1; if (parts.size() > 4) return false; }
        else return false;
    }
    return !parts.empty();
}
inline bool ValidVersion(const std::string& v) { std::vector<int> p; return ParseVersion(v, p); }
inline int CompareVersions(const std::string& a, const std::string& b) {
    std::vector<int> x, y;
    if (!ParseVersion(a, x) || !ParseVersion(b, y)) return 0;
    const size_t n = std::max(x.size(), y.size());
    x.resize(n, 0); y.resize(n, 0);
    for (size_t i = 0; i < n; ++i) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}
inline bool IsNewer(const std::string& candidate, const std::string& current) { return ValidVersion(candidate) && ValidVersion(current) && CompareVersions(candidate, current) > 0; }

// ---- feed parsing -------------------------------------------------------------------------------
// Reads the first string value for "key". Handles the escapes a JSON writer produces; it is not a general parser.
inline std::string JsonString(const std::string& text, const char* key) {
    const std::string pattern = std::string("\"") + key + "\"";
    size_t at = text.find(pattern);
    while (at != std::string::npos) {
        size_t i = at + pattern.size();
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) ++i;
        if (i < text.size() && text[i] == ':') {
            ++i;
            while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r' || text[i] == '\n')) ++i;
            if (i < text.size() && text[i] == '"') {
                std::string out;
                for (++i; i < text.size() && text[i] != '"'; ++i) {
                    if (text[i] != '\\' || i + 1 >= text.size()) { out += text[i]; continue; }
                    const char e = text[++i];
                    switch (e) {
                        case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': break;
                        case 'u': if (i + 4 < text.size()) { const unsigned cp = unsigned(std::strtoul(text.substr(i + 1, 4).c_str(), nullptr, 16)); out += cp < 128 ? char(cp) : '?'; i += 4; } break;
                        default: out += e; break;
                    }
                }
                return out;
            }
        }
        at = text.find(pattern, at + 1);
    }
    return {};
}
inline bool IsHex64(const std::string& s) { return s.size() == 64 && std::all_of(s.begin(), s.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }); }
inline bool StartsWithHttps(const std::string& s) { return s.rfind("https://", 0) == 0; }
// allowLocal: a test feed may point at a local zip. Production feeds must use https.
inline bool ParseFeed(const std::string& json, bool allowLocal, Feed& out, std::string& error) {
    Feed f;
    f.version = JsonString(json, "version"); f.url = JsonString(json, "url"); f.sha256 = JsonString(json, "sha256"); f.notes = JsonString(json, "notes");
    if (!ValidVersion(f.version)) { error = "the feed has no valid version"; return false; }
    if (!IsHex64(f.sha256)) { error = "the feed has no valid SHA-256, so the download cannot be verified"; return false; }
    // Production feeds must use https. A test feed may also name a plain local path, never another URL scheme.
    const bool plainPath = f.url.find("://") == std::string::npos;
    if (f.url.empty() || !(StartsWithHttps(f.url) || (allowLocal && plainPath))) { error = "the feed's download address is not https"; return false; }
    std::transform(f.sha256.begin(), f.sha256.end(), f.sha256.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    if (f.notes.size() > 4000) f.notes.resize(4000);
    out = f;
    return true;
}

// ---- hashing ------------------------------------------------------------------------------------
inline std::string Sha256File(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
    std::string hex;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0) {
        std::vector<char> buffer(1 << 16);
        bool ok = true;
        while (in) { in.read(buffer.data(), std::streamsize(buffer.size())); const auto n = in.gcount(); if (n > 0 && BCryptHashData(hash, reinterpret_cast<PUCHAR>(buffer.data()), ULONG(n), 0) < 0) { ok = false; break; } }
        unsigned char digest[32]{};
        if (ok && in.eof() && BCryptFinishHash(hash, digest, sizeof digest, 0) >= 0) {
            char two[3];
            for (unsigned char b : digest) { std::snprintf(two, sizeof two, "%02x", b); hex += two; }
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return hex;
}

// ---- settings and folders -----------------------------------------------------------------------
inline fs::path DataRoot() {
    wchar_t override_[MAX_PATH]{};   // tests point the updater at a scratch folder
    if (const DWORD o = GetEnvironmentVariableW(L"RARESTAGE_DATA_DIR", override_, MAX_PATH); o > 0 && o < MAX_PATH) return fs::path(override_);
    wchar_t buffer[MAX_PATH]{};
    const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, MAX_PATH);
    return (n > 0 && n < MAX_PATH ? fs::path(buffer) : fs::temp_directory_path()) / L"RareStage";
}
inline fs::path UpdatesRoot() { return DataRoot() / L"updates"; }
inline fs::path SettingsFile() { return DataRoot() / L"updater.ini"; }
inline std::string ReadSetting(const wchar_t* key, const std::string& fallback = {}) {
    wchar_t buffer[2048]{};
    GetPrivateProfileStringW(L"Updater", key, L"", buffer, 2048, SettingsFile().c_str());
    if (!buffer[0]) return fallback;
    return fs::path(buffer).string();
}
inline void WriteSetting(const wchar_t* key, const std::wstring& value) {
    std::error_code ec; fs::create_directories(DataRoot(), ec);
    WritePrivateProfileStringW(L"Updater", key, value.empty() ? nullptr : value.c_str(), SettingsFile().c_str());
}
inline std::string FeedUrl() { return ReadSetting(L"Url", DefaultFeed); }
inline bool AutoCheck() { return GetPrivateProfileIntW(L"Updater", L"Auto", 1, SettingsFile().c_str()) != 0; }
inline void SetAutoCheck(bool on) { WriteSetting(L"Auto", on ? L"1" : L"0"); }
inline std::string Skipped() { return ReadSetting(L"Skip"); }
inline void SetSkipped(const std::string& v) { WriteSetting(L"Skip", std::wstring(v.begin(), v.end())); }
inline int KeepCount() { return std::clamp(int(GetPrivateProfileIntW(L"Updater", L"Keep", 2, SettingsFile().c_str())), 0, 5); }

// ---- versions on disk ---------------------------------------------------------------------------
// The folder inside a version directory that holds the program: the directory itself, or its single child when the
// zip carried a top-level folder (as the CI package does).
inline std::optional<fs::path> ProgramDir(const fs::path& versionDir) {
    std::error_code ec;
    if (fs::exists(versionDir / L"RareStageConsole.exe", ec)) return versionDir;
    std::optional<fs::path> found; int count = 0;
    for (const auto& e : fs::directory_iterator(versionDir, ec)) if (e.is_directory(ec)) { ++count; if (fs::exists(e.path() / L"RareStageConsole.exe", ec)) found = e.path(); }
    return count == 1 ? found : std::nullopt;
}
// Complete versions (the "ok" marker is written last), oldest first.
inline std::vector<std::string> ReadyVersions(const fs::path& root) {
    std::vector<std::string> out; std::error_code ec;
    if (!fs::exists(root, ec)) return out;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        const std::string name = e.path().filename().string();
        if (e.is_directory(ec) && ValidVersion(name) && fs::exists(e.path() / L"ok", ec) && ProgramDir(e.path())) out.push_back(name);
    }
    std::sort(out.begin(), out.end(), [](const std::string& a, const std::string& b) { return CompareVersions(a, b) < 0; });
    return out;
}
inline std::optional<fs::path> NewerConsole(const fs::path& root, const std::string& current) {
    const auto ready = ReadyVersions(root);
    for (auto it = ready.rbegin(); it != ready.rend(); ++it)
        if (CompareVersions(*it, current) > 0) { if (const auto dir = ProgramDir(root / *it)) return *dir / L"RareStageConsole.exe"; }
    return std::nullopt;
}
// Keeps the newest few older versions for rollback; clears the rest, leftovers and half-unpacked folders.
inline void CleanOldVersions(const fs::path& root, const std::string& current, int keep) {
    std::error_code ec;
    if (!fs::exists(root, ec)) return;
    const auto ready = ReadyVersions(root);
    int kept = 0;
    for (auto it = ready.rbegin(); it != ready.rend(); ++it) if (CompareVersions(*it, current) < 0 && ++kept > keep) fs::remove_all(root / *it, ec);
    for (const auto& e : fs::directory_iterator(root, ec)) {
        const std::string name = e.path().filename().string();
        const bool isReady = std::find(ready.begin(), ready.end(), name) != ready.end();
        if (e.is_directory(ec) && !isReady && (name.find(".tmp") != std::string::npos || (ValidVersion(name) && CompareVersions(name, current) < 0))) fs::remove_all(e.path(), ec);
        else if (e.is_regular_file(ec) && (e.path().extension() == L".zip" || e.path().extension() == L".part")) fs::remove(e.path(), ec);
    }
}
inline std::vector<std::string> RollbackTargets(const fs::path& root, const std::string& current) {
    std::vector<std::string> out;
    for (const auto& v : ReadyVersions(root)) if (CompareVersions(v, current) < 0) out.insert(out.begin(), v);
    return out;
}

// ---- downloading --------------------------------------------------------------------------------
// https only. GitHub answers release downloads with a redirect to its CDN; https-to-http redirects are refused.
inline bool HttpDownload(const std::string& url, const fs::path& file, std::uintmax_t maxBytes, std::atomic<float>* progress, std::string& error, const std::atomic<bool>* cancel = nullptr) {
    if (!StartsWithHttps(url)) { error = "downloads must use https"; return false; }
    const std::wstring wide(url.begin(), url.end());
    wchar_t host[256]{}, path[2048]{}, extra[2048]{};
    URL_COMPONENTS parts{}; parts.dwStructSize = sizeof parts;
    parts.lpszHostName = host; parts.dwHostNameLength = 256; parts.lpszUrlPath = path; parts.dwUrlPathLength = 2048; parts.lpszExtraInfo = extra; parts.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) { error = "the address is not valid"; return false; }
    const std::wstring target = std::wstring(path) + extra;
    HINTERNET session = WinHttpOpen(L"RareStageConsole-Updater/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) { error = "could not start the network session"; return false; }
    WinHttpSetTimeouts(session, 15000, 15000, 30000, 30000);   // stalls are fatal; total time is not
    HINTERNET connection = WinHttpConnect(session, host, parts.nPort, 0);
    HINTERNET request = connection ? WinHttpOpenRequest(connection, L"GET", target.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;
    bool ok = false;
    if (request) {
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof policy);
        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(request, nullptr)) {
            error = "could not reach the server (error " + std::to_string(GetLastError()) + ")";
        } else {
            DWORD status = 0, size = sizeof status;
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
            wchar_t lengthText[32]{}; DWORD lengthSize = sizeof lengthText; std::uintmax_t total = 0;
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX, lengthText, &lengthSize, WINHTTP_NO_HEADER_INDEX)) total = _wcstoui64(lengthText, nullptr, 10);
            if (status != 200) error = "the server answered " + std::to_string(status);
            else if (total > maxBytes) error = "the download is larger than allowed";
            else {
                std::ofstream out(file, std::ios::binary | std::ios::trunc);
                std::vector<char> buffer(1 << 16); std::uintmax_t got = 0; ok = out.good();
                if (!ok) error = "could not write the download";
                for (DWORD n = 0; ok && WinHttpReadData(request, buffer.data(), DWORD(buffer.size()), &n) && n > 0;) {
                    got += n;
                    if (cancel && cancel->load()) { error = "cancelled"; ok = false; break; }
                    if (got > maxBytes) { error = "the download is larger than allowed"; ok = false; break; }
                    out.write(buffer.data(), n);
                    if (!out.good()) { error = "could not write the download"; ok = false; break; }
                    if (progress && total) progress->store(float(double(got) / double(total)));
                }
                if (ok && total && got != total) { error = "the download was cut short"; ok = false; }
            }
        }
    } else error = "could not open the request";
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    if (!ok) { std::error_code ec; fs::remove(file, ec); }
    return ok;
}

// Unpacks with tar.exe, which ships with Windows 10 and reads zip archives.
inline bool Unpack(const fs::path& zip, const fs::path& dest, std::string& error) {
    std::error_code ec; fs::create_directories(dest, ec);
    std::wstring command = L"tar.exe -xf \"" + zip.wstring() + L"\" -C \"" + dest.wstring() + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { error = "could not start tar.exe"; return false; }
    WaitForSingleObject(pi.hProcess, 120000);
    DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    if (code != 0) { error = "the update could not be unpacked"; return false; }
    return true;
}
// Moves a single top-level folder's contents up, so the version folder holds the program directly.
inline void Flatten(const fs::path& dir) {
    std::error_code ec;
    if (fs::exists(dir / L"RareStageConsole.exe", ec)) return;
    const auto inner = ProgramDir(dir);
    if (!inner || *inner == dir) return;
    for (const auto& e : fs::directory_iterator(*inner, ec)) fs::rename(e.path(), dir / e.path().filename(), ec);
    fs::remove_all(*inner, ec);
}

// ---- the updater --------------------------------------------------------------------------------
class Updater {
public:
    enum class State { Idle, Checking, UpToDate, Available, Downloading, Installing, Ready, Failed };
    explicit Updater(std::string currentVersion) : current_(std::move(currentVersion)) {}
    ~Updater() { cancel_ = true; join(); }
    Updater(const Updater&) = delete; Updater& operator=(const Updater&) = delete;

    State state() const { return state_.load(); }
    float progress() const { return progress_.load(); }
    bool busy() const { const State s = state_.load(); return s == State::Checking || s == State::Downloading || s == State::Installing; }
    std::string message() const { std::lock_guard<std::mutex> l(m_); return message_; }
    Feed available() const { std::lock_guard<std::mutex> l(m_); return feed_; }
    const std::string& current() const { return current_; }

    // manual: the operator asked, so a skipped release is offered again and a failure is reported.
    void check(bool manual) {
        if (busy()) return;
        join();
        state_ = State::Checking; progress_ = 0;
        setMessage(manual ? "Checking for updates..." : "");
        worker_ = std::thread([this, manual] { runCheck(manual); });
    }
    // Downloads, verifies and unpacks the version found by the last check. It is used on the next start.
    void install() {
        if (busy() || state_.load() != State::Available) return;
        join();
        state_ = State::Downloading; progress_ = 0;
        worker_ = std::thread([this] { runInstall(); });
    }
    void skipThisVersion() { const Feed f = available(); if (!f.version.empty()) SetSkipped(f.version); state_ = State::UpToDate; setMessage("Skipping " + f.version + "."); }

private:
    std::string current_;
    mutable std::mutex m_; std::string message_; Feed feed_;
    std::atomic<State> state_{State::Idle}; std::atomic<float> progress_{0}; std::atomic<bool> cancel_{false};
    std::thread worker_;

    void join() { if (worker_.joinable()) worker_.join(); }
    void setMessage(const std::string& text) { std::lock_guard<std::mutex> l(m_); message_ = text; }
    void fail(const std::string& why, bool show = true) { setMessage(show ? "Update failed: " + why : ""); state_ = show ? State::Failed : State::Idle; }

    static bool ReadFile(const fs::path& p, std::string& out) {
        std::ifstream in(p, std::ios::binary); if (!in) return false;
        out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        return out.size() <= MaxFeedBytes;
    }
    void runCheck(bool manual) {
        const std::string url = FeedUrl();
        const bool local = !StartsWithHttps(url);
        std::string text, error;
        const fs::path temp = fs::temp_directory_path() / (L"rarestage-feed-" + std::to_wstring(GetCurrentProcessId()) + L".json");
        bool got = false;
        if (local) got = ReadFile(fs::path(url), text);
        else {
            for (int attempt = 0; attempt < 2 && !got; ++attempt) {   // one quiet retry covers a flaky connection at startup
                got = HttpDownload(url, temp, MaxFeedBytes, nullptr, error, &cancel_) && ReadFile(temp, text);
                if (!got && attempt == 0) Sleep(3000);
            }
            std::error_code ec; fs::remove(temp, ec);
        }
        if (!got) { fail(error.empty() ? "the feed could not be read" : error, manual); return; }
        Feed feed;
        if (!ParseFeed(text, local, feed, error)) { fail(error, manual); return; }
        {
            std::lock_guard<std::mutex> l(m_); feed_ = feed;
        }
        if (!IsNewer(feed.version, current_)) { setMessage(manual ? "RareStage " + current_ + " is up to date." : ""); state_ = State::UpToDate; return; }
        if (!manual && Skipped() == feed.version) { setMessage(""); state_ = State::UpToDate; return; }
        if (Skipped() == feed.version) SetSkipped({});
        if (fs::exists(UpdatesRoot() / fs::path(feed.version) / L"ok")) { setMessage(feed.version + " is already installed. Restart the console to use it."); state_ = State::Ready; return; }
        setMessage("RareStage " + feed.version + " is available.");
        state_ = State::Available;
    }
    void runInstall() {
        const Feed feed = available();
        std::error_code ec;
        const fs::path root = UpdatesRoot(); fs::create_directories(root, ec);
        const fs::path zip = root / (fs::path(feed.version).wstring() + L".zip.part"), tmp = root / (fs::path(feed.version).wstring() + L".tmp"), dest = root / fs::path(feed.version);
        setMessage("Downloading " + feed.version + "...");
        std::string error;
        if (StartsWithHttps(feed.url)) { if (!HttpDownload(feed.url, zip, MaxZipBytes, &progress_, error, &cancel_)) { fail(error); return; } }
        else { fs::copy_file(fs::path(feed.url), zip, fs::copy_options::overwrite_existing, ec); if (ec) { fail("the local update file could not be read"); return; } }
        // The checksum is required: a download that does not match the feed is discarded.
        state_ = State::Installing; setMessage("Verifying " + feed.version + "...");
        if (Sha256File(zip) != feed.sha256) { fs::remove(zip, ec); fail("the download does not match its checksum and was discarded"); return; }
        setMessage("Installing " + feed.version + "...");
        fs::remove_all(tmp, ec);
        if (!Unpack(zip, tmp, error)) { fs::remove(zip, ec); fs::remove_all(tmp, ec); fail(error); return; }
        fs::remove(zip, ec);
        Flatten(tmp);
        if (!fs::exists(tmp / L"RareStageConsole.exe", ec) || !fs::exists(tmp / L"RareStage.exe", ec)) { fs::remove_all(tmp, ec); fail("the update does not contain RareStage and its console"); return; }
        fs::remove_all(dest, ec);
        fs::rename(tmp, dest, ec);
        if (ec) { fs::remove_all(tmp, ec); fail("could not finish installing the update"); return; }
        { std::ofstream ok(dest / L"ok"); ok << feed.version << "\n"; }   // written last: marks the version complete
        CleanOldVersions(root, current_, KeepCount());
        setMessage(feed.version + " is installed. Restart the console to use it.");
        state_ = State::Ready;
    }
};

// ---- launcher and rollback ----------------------------------------------------------------------
// A hand-over must never loop: not to the copy that is already running, and not twice in a row. The child of a
// hand-over inherits RARESTAGE_UPDATE_LAUNCHED and never hands over again.
inline bool HandOverAllowed(const fs::path& candidate) {
    wchar_t flag[8]{};
    if (GetEnvironmentVariableW(L"RARESTAGE_UPDATE_LAUNCHED", flag, 8) > 0) return false;
    wchar_t self[MAX_PATH]{};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::error_code ec;
    return !fs::equivalent(fs::path(self), candidate, ec);
}
// At start: if a newer complete version exists, run it with the same arguments and let the caller exit.
inline bool LaunchNewerIfAny(const std::string& current, const std::wstring& commandLine) {
    const auto exe = NewerConsole(UpdatesRoot(), current);
    if (!exe || !HandOverAllowed(*exe)) return false;
    SetEnvironmentVariableW(L"RARESTAGE_UPDATE_LAUNCHED", L"1");
    std::wstring command = L"\"" + exe->wstring() + L"\" " + commandLine;
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr, exe->parent_path().c_str(), &si, &pi)) return false;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return true;
}
// Removes every version newer than `target` after this process has exited, then starts the console again, so the
// launcher falls back to `target`. The skipped marker keeps the release you left from coming straight back.
inline bool Rollback(const std::string& current, const std::string& target, std::string& error) {
    const auto targets = RollbackTargets(UpdatesRoot(), current);
    if (std::find(targets.begin(), targets.end(), target) == targets.end()) { error = "That version is no longer on this computer."; return false; }
    std::wstring script = L"@echo off\r\nping -n 4 127.0.0.1 >nul\r\n";
    std::string newest;
    for (const auto& v : ReadyVersions(UpdatesRoot())) if (CompareVersions(v, target) > 0) { script += L"rmdir /s /q \"" + (UpdatesRoot() / v).wstring() + L"\"\r\n"; newest = v; }
    if (newest.empty()) { error = "Nothing newer than that version is installed."; return false; }
    const auto next = ProgramDir(UpdatesRoot() / target);
    if (next) script += L"start \"\" \"" + (*next / L"RareStageConsole.exe").wstring() + L"\"\r\n";
    script += L"del \"%~f0\"\r\n";
    const fs::path batch = fs::temp_directory_path() / L"rarestage-rollback.bat";
    { std::ofstream out(batch, std::ios::binary | std::ios::trunc); const std::string narrow = fs::path(script).string(); out.write(narrow.data(), std::streamsize(narrow.size())); if (!out.good()) { error = "Could not write the rollback script."; return false; } }
    std::wstring command = L"cmd.exe /c \"" + batch.wstring() + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { error = "Could not start the rollback script."; return false; }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    SetSkipped(newest);
    return true;
}
}  // namespace rarestage::update
