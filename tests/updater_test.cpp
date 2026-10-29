#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "console_updater.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>
using namespace rarestage::update;
namespace fs = std::filesystem;
static int failures = 0;
static void check(const std::string& name, bool ok) { if (!ok) { std::printf("FAIL %s\n", name.c_str()); ++failures; } }

static void WriteText(const fs::path& p, const std::string& text) { std::error_code ec; fs::create_directories(p.parent_path(), ec); std::ofstream(p, std::ios::binary) << text; }
static void Touch(const fs::path& p) { WriteText(p, "x"); }
static bool Zip(const fs::path& sourceDir, const fs::path& zipFile) {
    std::wstring cmd = L"tar.exe -a -cf \"" + zipFile.wstring() + L"\" -C \"" + sourceDir.parent_path().wstring() + L"\" \"" + sourceDir.filename().wstring() + L"\"";
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
    WaitForSingleObject(pi.hProcess, 60000); DWORD code = 1; GetExitCodeProcess(pi.hProcess, &code); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return code == 0;
}
static std::string FeedJson(const std::string& version, const std::string& url, const std::string& sha, const std::string& notes = "Notes") {
    std::string u; for (char c : url) { if (c == '\\') u += "\\\\"; else u += c; }
    return "{\n \"version\": \"" + version + "\",\n \"url\": \"" + u + "\",\n \"sha256\": \"" + sha + "\",\n \"notes\": \"" + notes + "\"\n}\n";
}
static void Wait(Updater& u) { const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(60); while (u.busy() && std::chrono::steady_clock::now() < limit) std::this_thread::sleep_for(std::chrono::milliseconds(10)); }

int main(int argc, char** argv) {
    // Optional manual check of the real WinHTTP path: updater_test --net <https url>
    if (argc >= 3 && std::string(argv[1]) == "--net") {
        const fs::path out = fs::temp_directory_path() / L"rarestage-net-test.bin"; std::string err; std::atomic<float> p{0};
        const bool ok = HttpDownload(argv[2], out, 50ull * 1024 * 1024, &p, err);
        std::error_code ec; const auto size = ok ? fs::file_size(out, ec) : 0;
        std::printf("net download %s: %s, %llu bytes\n", argv[2], ok ? "ok" : err.c_str(), (unsigned long long)size);
        std::printf("sha256 %s\n", ok ? Sha256File(out).c_str() : "-");
        fs::remove(out, ec); return ok ? 0 : 1;
    }

    // Manual end-to-end check over the real network: updater_test --e2e <data dir> <feed file>
    // The feed file is local (so it can announce any version) but its download address may be a real https release asset.
    if (argc >= 4 && std::string(argv[1]) == "--e2e") {
        SetEnvironmentVariableW(L"RARESTAGE_DATA_DIR", fs::path(argv[2]).c_str());
        // "default" uses the real production feed (the latest GitHub release); anything else is a local feed file.
        WriteSetting(L"Url", std::string(argv[3]) == "default" ? std::wstring() : fs::path(argv[3]).wstring());
        Updater u(argc >= 5 ? argv[4] : "0.31.0");
        u.check(true); Wait(u);
        std::printf("check: state %d, %s\n", int(u.state()), u.message().c_str());
        if (u.state() != Updater::State::Available) return 2;
        u.install();
        while (u.busy()) { std::printf("\r%s %3.0f%%   ", u.message().c_str(), double(u.progress()) * 100.0); std::this_thread::sleep_for(std::chrono::milliseconds(200)); }
        std::printf("\ninstall: state %d, %s\n", int(u.state()), u.message().c_str());
        const auto next = NewerConsole(UpdatesRoot(), argc >= 5 ? argv[4] : "0.31.0");
        std::printf("launcher would run: %s\n", next ? next->string().c_str() : "(nothing)");
        return u.state() == Updater::State::Ready && next ? 0 : 3;
    }

    // Everything below runs against a scratch data folder, never the real one.
    const fs::path scratch = fs::temp_directory_path() / (L"rarestage-updater-test-" + std::to_wstring(GetCurrentProcessId()));
    std::error_code ec; fs::remove_all(scratch, ec); fs::create_directories(scratch, ec);
    SetEnvironmentVariableW(L"RARESTAGE_DATA_DIR", (scratch / L"data").c_str());

    // ---- versions
    check("newer_minor", IsNewer("0.32.0", "0.31.0")); check("equal_is_not_newer", !IsNewer("0.31.0", "0.31.0"));
    check("newer_patch", IsNewer("0.31.1", "0.31.0")); check("short_form_equal", !IsNewer("0.31", "0.31.0") && !IsNewer("0.31.0", "0.31"));
    check("v_prefix", IsNewer("v0.32.0", "0.31.9")); check("numeric_not_textual", IsNewer("0.100.0", "0.99.0") && !IsNewer("0.99.0", "0.100.0"));
    check("older_is_not_newer", !IsNewer("0.30.9", "0.31.0"));
    check("garbage_is_not_a_version", !ValidVersion("abc") && !ValidVersion("") && !ValidVersion("0.31.0-beta") && !ValidVersion("..") && !ValidVersion("1..2") && !ValidVersion("0.1.2.3.4.5") && !ValidVersion("99999999999"));
    check("garbage_is_never_newer", !IsNewer("abc", "0.1") && !IsNewer("0.2", "abc"));
    check("compare_orders", CompareVersions("0.30.0", "0.31.0") < 0 && CompareVersions("0.31.0", "0.30.0") > 0 && CompareVersions("1.0", "1.0.0") == 0);

    // ---- feed parsing
    check("json_reads_string", JsonString("{\"version\": \"0.32.0\"}", "version") == "0.32.0");
    check("json_missing_key", JsonString("{\"a\": \"b\"}", "version").empty());
    check("json_escapes", JsonString("{\"notes\":\"a\\\"b\\nc\\\\d\\u0041\"}", "notes") == "a\"b\nc\\dA");
    check("json_key_inside_a_value_is_not_the_key", JsonString("{\"notes\":\"mentions \\\"version\\\": \\\"9\\\"\",\"version\":\"0.32.0\"}", "version") == "0.32.0");
    const std::string sha(64, 'a');
    Feed feed; std::string error;
    check("feed_valid", ParseFeed(FeedJson("0.32.0", "https://example.com/a.zip", sha), false, feed, error) && feed.version == "0.32.0" && feed.url == "https://example.com/a.zip" && feed.sha256 == sha);
    check("feed_normalises_sha_case", ParseFeed(FeedJson("0.32.0", "https://example.com/a.zip", std::string(64, 'A')), false, feed, error) && feed.sha256 == sha);
    check("feed_rejects_http", !ParseFeed(FeedJson("0.32.0", "http://example.com/a.zip", sha), false, feed, error));
    check("test_feed_still_refuses_other_url_schemes", !ParseFeed(FeedJson("0.32.0", "http://example.com/a.zip", sha), true, feed, error) && !ParseFeed(FeedJson("0.32.0", "ftp://example.com/a.zip", sha), true, feed, error) && !ParseFeed(FeedJson("0.32.0", "file:///C:/a.zip", sha), true, feed, error));
    check("feed_rejects_local_path_in_production", !ParseFeed(FeedJson("0.32.0", "C:\\x\\a.zip", sha), false, feed, error));
    check("feed_allows_local_path_for_tests", ParseFeed(FeedJson("0.32.0", "C:\\x\\a.zip", sha), true, feed, error) && feed.url == "C:\\x\\a.zip");
    check("feed_rejects_missing_sha", !ParseFeed(FeedJson("0.32.0", "https://example.com/a.zip", ""), false, feed, error));
    check("feed_rejects_short_sha", !ParseFeed(FeedJson("0.32.0", "https://example.com/a.zip", "abc"), false, feed, error));
    check("feed_rejects_non_hex_sha", !ParseFeed(FeedJson("0.32.0", "https://example.com/a.zip", std::string(64, 'z')), false, feed, error));
    check("feed_rejects_bad_version", !ParseFeed(FeedJson("latest", "https://example.com/a.zip", sha), false, feed, error));
    check("feed_rejects_empty", !ParseFeed("", false, feed, error) && !ParseFeed("not json", false, feed, error));
    check("feed_truncates_huge_notes", ParseFeed(FeedJson("0.32.0", "https://example.com/a.zip", sha, std::string(9000, 'n')), false, feed, error) && feed.notes.size() <= 4000);

    // ---- hashing
    WriteText(scratch / L"abc.txt", "abc"); WriteText(scratch / L"empty.txt", "");
    check("sha256_known_vector", Sha256File(scratch / L"abc.txt") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    check("sha256_empty", Sha256File(scratch / L"empty.txt") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    check("sha256_missing_file", Sha256File(scratch / L"nope.txt").empty());

    // ---- network rules that do not need a network
    {
        std::atomic<float> p{0}; std::string err;
        check("download_refuses_http", !HttpDownload("http://example.com/a.zip", scratch / L"d.bin", 1000, &p, err) && !fs::exists(scratch / L"d.bin"));
        check("download_refuses_garbage", !HttpDownload("not a url", scratch / L"d.bin", 1000, &p, err));
        check("download_refuses_file_scheme", !HttpDownload("file:///C:/Windows/notepad.exe", scratch / L"d.bin", 1000, &p, err));
    }

    // ---- versions on disk
    {
        const fs::path root = scratch / L"disk";
        Touch(root / L"0.30.0" / L"RareStageConsole.exe"); Touch(root / L"0.30.0" / L"RareStage.exe"); Touch(root / L"0.30.0" / L"ok");
        Touch(root / L"0.31.5" / L"RareStage" / L"RareStageConsole.exe"); Touch(root / L"0.31.5" / L"ok");          // nested, as a zip with a top folder
        Touch(root / L"0.32.0" / L"RareStageConsole.exe");                                                          // unpacked but no ok marker
        Touch(root / L"0.33.0.tmp" / L"RareStageConsole.exe"); Touch(root / L"junk" / L"ok"); Touch(root / L"0.29.0.zip.part"); Touch(root / L"0.28.0.zip");
        Touch(root / L"0.20.0" / L"ok");                                                                            // marker but no program
        const auto ready = ReadyVersions(root);
        check("ready_versions_need_marker_and_program", ready == std::vector<std::string>{"0.30.0", "0.31.5"});
        const auto next = NewerConsole(root, "0.31.0");
        check("newer_console_found_through_nested_folder", next && next->filename() == L"RareStageConsole.exe" && next->parent_path().filename() == L"RareStage");
        check("no_newer_when_already_newest", !NewerConsole(root, "0.31.5") && !NewerConsole(root, "0.40.0"));
        check("incomplete_version_is_never_launched", !NewerConsole(root, "0.31.9"));
        check("rollback_targets_are_older_ready_versions", RollbackTargets(root, "0.31.5") == std::vector<std::string>{"0.30.0"} && RollbackTargets(root, "0.30.0").empty());
        CleanOldVersions(root, "0.31.5", 0);
        check("clean_removes_old_and_leftovers", !fs::exists(root / L"0.30.0") && !fs::exists(root / L"0.33.0.tmp") && !fs::exists(root / L"0.29.0.zip.part") && !fs::exists(root / L"0.28.0.zip") && !fs::exists(root / L"0.20.0"));
        check("clean_keeps_current_and_in_progress_newer", fs::exists(root / L"0.31.5") && fs::exists(root / L"0.32.0"));
        Touch(root / L"0.29.5" / L"RareStageConsole.exe"); Touch(root / L"0.29.5" / L"ok"); Touch(root / L"0.30.5" / L"RareStageConsole.exe"); Touch(root / L"0.30.5" / L"ok"); Touch(root / L"0.31.0" / L"RareStageConsole.exe"); Touch(root / L"0.31.0" / L"ok");
        CleanOldVersions(root, "0.31.5", 2);
        check("clean_keeps_the_newest_older_versions", !fs::exists(root / L"0.29.5") && fs::exists(root / L"0.30.5") && fs::exists(root / L"0.31.0"));
    }

    // ---- hand-over guards
    {
        const fs::path other = scratch / L"other" / L"RareStageConsole.exe"; Touch(other);
        SetEnvironmentVariableW(L"RARESTAGE_UPDATE_LAUNCHED", nullptr);
        check("handover_allowed_to_a_different_copy", HandOverAllowed(other));
        wchar_t self[MAX_PATH]{}; GetModuleFileNameW(nullptr, self, MAX_PATH);
        check("handover_refused_to_the_running_copy", !HandOverAllowed(fs::path(self)));
        SetEnvironmentVariableW(L"RARESTAGE_UPDATE_LAUNCHED", L"1");
        check("handover_refused_twice_in_a_row", !HandOverAllowed(other));
        SetEnvironmentVariableW(L"RARESTAGE_UPDATE_LAUNCHED", nullptr);
    }

    // ---- the whole cycle against a local feed
    {
        const fs::path source = scratch / L"pkg" / L"RareStage";
        Touch(source / L"RareStageConsole.exe"); Touch(source / L"RareStage.exe"); Touch(source / L"assets" / L"rare-logo.png");
        const fs::path zip = scratch / L"RareStage-windows-x64.zip";
        check("test_zip_built", Zip(source, zip));
        const std::string good = Sha256File(zip);
        const fs::path feedFile = scratch / L"latest.json";
        auto useFeed = [&](const std::string& json) { WriteText(feedFile, json); WriteSetting(L"Url", feedFile.wstring()); };
        auto fresh = [&] { fs::remove_all(UpdatesRoot(), ec); WriteSetting(L"Skip", L""); };

        // newer version: found, installed, verified, usable, never run until restart
        fresh(); useFeed(FeedJson("0.32.0", zip.string(), good, "Better things"));
        {
            Updater u("0.31.0"); u.check(true); Wait(u);
            check("cycle_finds_the_update", u.state() == Updater::State::Available && u.available().version == "0.32.0" && u.available().notes == "Better things");
            check("nothing_is_installed_before_the_operator_asks", !fs::exists(UpdatesRoot() / L"0.32.0"));
            u.install(); Wait(u);
            check("cycle_installs", u.state() == Updater::State::Ready);
            check("installed_flat_with_marker", fs::exists(UpdatesRoot() / L"0.32.0" / L"RareStageConsole.exe") && fs::exists(UpdatesRoot() / L"0.32.0" / L"RareStage.exe") && fs::exists(UpdatesRoot() / L"0.32.0" / L"assets" / L"rare-logo.png") && fs::exists(UpdatesRoot() / L"0.32.0" / L"ok"));
            check("zip_and_temp_are_cleaned", !fs::exists(UpdatesRoot() / L"0.32.0.tmp") && !fs::exists(UpdatesRoot() / L"0.32.0.zip.part"));
            check("launcher_would_pick_it", NewerConsole(UpdatesRoot(), "0.31.0").has_value() && !NewerConsole(UpdatesRoot(), "0.32.0"));
            u.check(true); Wait(u);
            check("checking_again_reports_it_ready", u.state() == Updater::State::Ready);
        }
        // same or older version: up to date
        fresh(); useFeed(FeedJson("0.31.0", zip.string(), good));
        { Updater u("0.31.0"); u.check(true); Wait(u); check("same_version_is_up_to_date", u.state() == Updater::State::UpToDate && u.message().find("up to date") != std::string::npos); }
        useFeed(FeedJson("0.30.0", zip.string(), good));
        { Updater u("0.31.0"); u.check(true); Wait(u); check("older_feed_is_not_offered", u.state() == Updater::State::UpToDate); }
        // skip: automatic checks ignore it, a manual check offers it again and forgets the skip
        fresh(); useFeed(FeedJson("0.32.0", zip.string(), good));
        {
            Updater u("0.31.0"); u.check(true); Wait(u); u.skipThisVersion();
            check("skip_is_remembered", Skipped() == "0.32.0");
            u.check(false); Wait(u); check("automatic_check_ignores_a_skipped_release", u.state() == Updater::State::UpToDate);
            u.check(true); Wait(u); check("manual_check_offers_it_again", u.state() == Updater::State::Available && Skipped().empty());
        }
        // a download that does not match its checksum is thrown away
        fresh(); useFeed(FeedJson("0.32.0", zip.string(), std::string(64, '0')));
        { Updater u("0.31.0"); u.check(true); Wait(u); u.install(); Wait(u);
          check("wrong_checksum_is_rejected", u.state() == Updater::State::Failed && u.message().find("checksum") != std::string::npos && !fs::exists(UpdatesRoot() / L"0.32.0") && !fs::exists(UpdatesRoot() / L"0.32.0.zip.part")); }
        // a zip that is not a zip, even with a matching checksum, installs nothing
        WriteText(scratch / L"corrupt.zip", "this is not a zip file");
        fresh(); useFeed(FeedJson("0.32.0", (scratch / L"corrupt.zip").string(), Sha256File(scratch / L"corrupt.zip")));
        { Updater u("0.31.0"); u.check(true); Wait(u); u.install(); Wait(u);
          check("corrupt_zip_installs_nothing", u.state() == Updater::State::Failed && !fs::exists(UpdatesRoot() / L"0.32.0") && !fs::exists(UpdatesRoot() / L"0.32.0.tmp")); }
        // a zip without the program inside is refused
        const fs::path hollow = scratch / L"hollow" / L"RareStage"; Touch(hollow / L"readme.txt");
        check("hollow_zip_built", Zip(hollow, scratch / L"hollow.zip"));
        fresh(); useFeed(FeedJson("0.32.0", (scratch / L"hollow.zip").string(), Sha256File(scratch / L"hollow.zip")));
        { Updater u("0.31.0"); u.check(true); Wait(u); u.install(); Wait(u);
          check("zip_without_the_program_is_refused", u.state() == Updater::State::Failed && !fs::exists(UpdatesRoot() / L"0.32.0")); }
        // feed problems: reported on a manual check, silent otherwise
        fs::remove(feedFile, ec); WriteSetting(L"Url", feedFile.wstring());
        { Updater u("0.31.0"); u.check(true); Wait(u); check("missing_feed_is_reported_when_asked", u.state() == Updater::State::Failed);
          u.check(false); Wait(u); check("missing_feed_is_silent_at_startup", u.state() == Updater::State::Idle && u.message().empty()); }
        useFeed("{ \"version\": \"0.32.0\", \"url\": \"http://insecure/a.zip\", \"sha256\": \"" + good + "\" }");
        { Updater u("0.31.0"); u.check(true); Wait(u); check("insecure_feed_is_refused", u.state() == Updater::State::Failed); }
        // install() before a successful check does nothing
        { Updater u("0.31.0"); u.install(); check("install_needs_a_found_update", u.state() == Updater::State::Idle && !u.busy()); }
        // the settings round trip
        SetAutoCheck(false); check("auto_check_setting", !AutoCheck()); SetAutoCheck(true); check("auto_check_default_on", AutoCheck());
        WriteSetting(L"Url", L""); check("feed_default_is_the_github_release", FeedUrl() == DefaultFeed && StartsWithHttps(FeedUrl()));
    }

    fs::remove_all(scratch, ec);
    if (failures == 0) std::printf("updater: all checks passed\n");
    return failures ? 1 : 0;
}
