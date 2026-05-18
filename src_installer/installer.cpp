#define MINIZ_HEADER_FILE_ONLY
#include "miniz.h"

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <algorithm>

#pragma comment(lib, "winhttp.lib")

inline void log_red(bool highlight = false)
{
	WORD color = FOREGROUND_RED;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

inline void log_green(bool highlight = false)
{
	WORD color = FOREGROUND_GREEN;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_blue(bool highlight = false)
{
	WORD color = FOREGROUND_BLUE;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_yellow(bool highlight = false)
{
	WORD color = FOREGROUND_RED | FOREGROUND_GREEN;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_default(bool highlight = false)
{
	WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
	if (highlight) {
		color |= FOREGROUND_INTENSITY;
	}
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
}

void log_error(const std::string& msg)
{
	log_red(true);
	std::cout << "\n[!] " << msg << "\n";
	log_default();
}

std::string open_file_dialog()
{
	char filename[MAX_PATH] = { 0 };

	OPENFILENAMEA ofn = { 0 };
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFilter = "GTA IV Executable\0GTAIV.exe\0All Files\0*.*\0";
	ofn.lpstrFile = filename;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrTitle = "Select your GTAIV.exe";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
	ofn.lpstrInitialDir = nullptr;

	if (GetOpenFileNameA(&ofn)) {
		return filename;
	}
		

	return "";
}

bool file_exists(const std::string& path) {
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
}

// Initialize zip archive from file path
bool init_zip_from_file(const std::filesystem::path& zip_path, mz_zip_archive& zip)
{
	// Convert to string for miniz API (miniz uses char* paths)
	std::string zip_path_str = zip_path.string();
	return mz_zip_reader_init_file(&zip, zip_path_str.c_str(), 0) == MZ_TRUE;
}

std::string read_file_from_zip(const std::filesystem::path& zip_path, const std::string& file_path_in_zip)
{
	mz_zip_archive zip = {};
	if (!init_zip_from_file(zip_path, zip)) {
		return "";
	}

	size_t file_size = 0;
	void* file_data = mz_zip_reader_extract_file_to_heap(&zip, file_path_in_zip.c_str(), &file_size, 0);
	
	std::string result;
	if (file_data && file_size > 0) 
	{
		result.assign(static_cast<const char*>(file_data), file_size);
		mz_free(file_data);
	}

	mz_zip_reader_end(&zip);
	return result;
}

std::string read_file_from_disk(const std::string& file_path)
{
	std::ifstream file(file_path);
	if (!file.is_open()) {
		return "";
	}
	
	std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return content;
}

// Trim whitespace from string
std::string trim_whitespace(const std::string& str)
{
	size_t first = str.find_first_not_of(" \t\n\r");
	if (first == std::string::npos) {
		return "";
	}

	size_t last = str.find_last_not_of(" \t\n\r");
	return str.substr(first, (last - first + 1));
}

// Parse version string like "1.1.8" into a vector of integers
std::vector<int> parse_version(const std::string& version_str)
{
	std::vector<int> parts;
	std::string current;
	
	for (char c : version_str) 
	{
		if (c == '.') 
		{
			if (!current.empty()) 
			{
				try {
					parts.push_back(std::stoi(current));
				} catch (...) {
					parts.push_back(0);
				}
				current.clear();
			}
		} 
		else if (isdigit(c)) {
			current += c;
		}
	}
	
	if (!current.empty()) 
	{
		try {
			parts.push_back(std::stoi(current));
		} catch (...) {
			parts.push_back(0);
		}
	}
	
	return parts;
}

// Compare two version vectors, returns true if v1 > v2
bool version_greater_than(const std::vector<int>& v1, const std::vector<int>& v2)
{
	size_t max_len = max(v1.size(), v2.size());
	
	for (size_t i = 0; i < max_len; i++) 
	{
		int a = (i < v1.size()) ? v1[i] : 0;
		int b = (i < v2.size()) ? v2[i] : 0;
		
		if (a > b) return true;
		if (a < b) return false;
	}
	
	return false; // equal
}

// Get short SHA (first 7 characters) from full SHA
std::string get_short_sha(const std::string& full_sha)
{
	if (full_sha.length() >= 7) {
		return full_sha.substr(0, 7);
	}
	return full_sha;
}

// Get the latest commit SHA from a GitHub repository
// Returns empty string on failure, or the commit SHA on success
std::string get_latest_github_commit_sha(const std::string& owner, const std::string& repo, const std::string& branch = "main")
{
	// GitHub API endpoint: https://api.github.com/repos/{owner}/{repo}/commits/{branch}
	std::string url = "https://api.github.com/repos/" + owner + "/" + repo + "/commits/" + branch;
	
	HINTERNET session = WinHttpOpen(L"GTAIV-Remix-Installer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		return "";
	}

	// Convert URL to wide string
	std::wstring wurl(url.begin(), url.end());
	HINTERNET connect = WinHttpConnect(session, L"api.github.com", INTERNET_DEFAULT_HTTPS_PORT, 0);
	
	if (!connect) {

		WinHttpCloseHandle(session);
		return "";
	}

	// Convert path to wide string
	std::wstring wpath = L"/repos/" + std::wstring(owner.begin(), owner.end()) + L"/" + std::wstring(repo.begin(), repo.end()) + L"/commits/" + std::wstring(branch.begin(), branch.end());
	HINTERNET request = WinHttpOpenRequest(connect, L"GET", wpath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	
	if (!request) 
	{
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return "";
	}

	// Add User-Agent header (GitHub API requires this)
	WinHttpAddRequestHeaders(request, L"User-Agent: GTAIV-Remix-Installer", -1, WINHTTP_ADDREQ_FLAG_ADD);

	// Send request
	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return "";
	}

	// Receive response
	if (!WinHttpReceiveResponse(request, nullptr))
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return "";
	}

	// Read response data
	std::string response;
	DWORD bytes_available = 0;
	DWORD bytes_read = 0;
	char buffer[4096] = { 0 };

	do 
	{
		if (!WinHttpQueryDataAvailable(request, &bytes_available)) {
			break;
		}

		if (bytes_available == 0) {
			break;
		}

		if (!WinHttpReadData(request, buffer, sizeof(buffer) - 1, &bytes_read)) {
			break;
		}

		if (bytes_read > 0) 
		{
			buffer[bytes_read] = '\0';
			response += buffer;
		}
	} while (bytes_read > 0);

	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);

	// Parse JSON response to extract SHA
	// GitHub API returns: {"sha":"abc123...","commit":{...},...}
	size_t sha_pos = response.find("\"sha\":\"");
	if (sha_pos == std::string::npos) {
		return "";
	}

	sha_pos += 7; // Skip past "sha":"
	size_t sha_end = response.find("\"", sha_pos);

	if (sha_end == std::string::npos) {
		return "";
	}

	return response.substr(sha_pos, sha_end - sha_pos);
}

// Compare local commit SHA file with GitHub latest commit
// Returns: -1 if local is newer/unknown, 0 if same, 1 if GitHub is newer
// Returns -2 on error (file doesn't exist, network error, etc.)
int compare_commit_sha(const std::string& local_sha_file_path, const std::string& github_owner, const std::string& github_repo, const std::string& branch = "main")
{
	// Read local SHA
	std::string local_sha = read_file_from_disk(local_sha_file_path);

	if (local_sha.empty()) {
		return -2; // File doesn't exist or is empty
	}

	local_sha = trim_whitespace(local_sha);

	// Get GitHub SHA
	std::string github_sha = get_latest_github_commit_sha(github_owner, github_repo, branch);

	if (github_sha.empty()) {
		return -2; // Network error or API failure
	}

	github_sha = trim_whitespace(github_sha);

	// Compare (case-insensitive)
	std::string local_lower = local_sha;
	std::string github_lower = github_sha;
	std::transform(local_lower.begin(), local_lower.end(), local_lower.begin(), ::tolower);
	std::transform(github_lower.begin(), github_lower.end(), github_lower.begin(), ::tolower);

	if (local_lower == github_lower) {
		return 0; // Same commit
	}

	// For simplicity, if they differ, assume GitHub is newer
	// (In a real scenario, you'd need to query commit dates or use Git API to determine order)
	return 1; // GitHub is newer (or different)
}

// Write string content to a file
bool write_file_to_disk(const std::string& file_path, const std::string& content)
{
	std::ofstream file(file_path);
	if (!file.is_open()) {
		return false;
	}

	file << content;
	return file.good();
}

// Download a file from URL to a local path with progress and speed display
bool download_file_to_path(const std::wstring& url, const std::filesystem::path& target_path)
{
	// Parse URL
	std::wstring host, path;
	size_t protocol_end = url.find(L"://");
	if (protocol_end == std::wstring::npos) {
		return false;
	}
	
	size_t host_start = protocol_end + 3;
	size_t path_start = url.find(L"/", host_start);

	if (path_start == std::wstring::npos) 
	{
		host = url.substr(host_start);
		path = L"/";
	} 
	else 
	{
		host = url.substr(host_start, path_start - host_start);
		path = url.substr(path_start);
	}

	HINTERNET session = WinHttpOpen(L"GTAIV-Remix-Installer/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		return false;
	}

	HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
	if (!connect) 
	{
		WinHttpCloseHandle(session);
		return false;
	}

	// Make the GET request to download the file
	HINTERNET request = WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
	if (!request)
	{
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Enable redirect following
	DWORD redirect_policy = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect_policy, sizeof(redirect_policy));

	// Add User-Agent header
	WinHttpAddRequestHeaders(request, L"User-Agent: GTAIV-Remix-Installer", -1, WINHTTP_ADDREQ_FLAG_ADD);

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	if (!WinHttpReceiveResponse(request, nullptr)) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Create target directory if needed
	try {
		std::filesystem::create_directories(target_path.parent_path());
	} 
	catch (...) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Open output file
	std::ofstream out_file(target_path, std::ios::binary);
	if (!out_file.is_open()) 
	{
		WinHttpCloseHandle(request);
		WinHttpCloseHandle(connect);
		WinHttpCloseHandle(session);
		return false;
	}

	// Read and write data with progress tracking
	DWORD bytes_available = 0;
	DWORD bytes_read = 0;
	char buffer[8192] = { 0 };
	bool success = true;
	
	ULONGLONG total_bytes_downloaded = 0;
	DWORD start_time = GetTickCount();
	DWORD last_update_time = start_time;

	log_yellow(true);
	do 
	{
		if (!WinHttpQueryDataAvailable(request, &bytes_available)) 
		{
			success = false;
			break;
		}

		if (bytes_available == 0) {
			break;
		}

		if (!WinHttpReadData(request, buffer, sizeof(buffer), &bytes_read)) 
		{
			success = false;
			break;
		}

		if (bytes_read > 0) 
		{
			out_file.write(buffer, bytes_read);
			if (!out_file.good()) 
			{
				success = false;
				break;
			}
			
			total_bytes_downloaded += bytes_read;
			DWORD current_time = GetTickCount();
			
			// Update progress every 500ms
			if (current_time - last_update_time >= 500) 
			{
				// Calculate speed (bytes per second)
				DWORD elapsed = current_time - start_time;
				float speed = 0.0f;

				if (elapsed > 0) {
					speed = (float)total_bytes_downloaded / (float)elapsed * 1000.0f; // bytes per second
				}

				float speed_MBps = speed / (1024.0f * 1024.0f);
				float downloaded_MB = (float)total_bytes_downloaded / (1024.0f * 1024.0f);
				
				// Display downloaded size and speed
				std::cout 
					<< "\rDownloading: " << std::fixed << std::setprecision(1) 
					<< downloaded_MB << " MB - " << speed_MBps << " MB/s    ";

				std::cout.flush();
				
				last_update_time = current_time;
			}
		}
	} while (bytes_read > 0);

	log_default();

	// Final progress update
	if (success && total_bytes_downloaded > 0)
	{
		float downloadedMB = (float)total_bytes_downloaded / (1024.0f * 1024.0f);
		DWORD elapsed = GetTickCount() - start_time;
		float speed = 0.0f;

		if (elapsed > 0) {
			speed = (float)total_bytes_downloaded / (float)elapsed * 1000.0f;
		}

		float speed_MBps = speed / (1024.0f * 1024.0f);

		std::cout 
			<< "\rDownloaded: " << std::fixed << std::setprecision(1) 
			<< downloadedMB << " MB - " << speed_MBps << " MB/s    \n";

		std::cout.flush();
	}

	out_file.close();
	WinHttpCloseHandle(request);
	WinHttpCloseHandle(connect);
	WinHttpCloseHandle(session);

	return success;
}

bool extract_single_file_from_zip(const std::filesystem::path& zip_path, const std::string& file_path_in_zip, const std::filesystem::path& target_path)
{
	mz_zip_archive zip = {};
	if (!init_zip_from_file(zip_path, zip)) {
		return false;
	}

	// Find the file in the zip
	int file_index = mz_zip_reader_locate_file(&zip, file_path_in_zip.c_str(), nullptr, 0);
	if (file_index < 0) {
		mz_zip_reader_end(&zip);
		return false;
	}

	// Extract directly to file (more efficient for large files)
	std::string target_path_str = target_path.string();
	if (mz_zip_reader_extract_to_file(&zip, file_index, target_path_str.c_str(), 0)) 
	{
		mz_zip_reader_end(&zip);
		return true;
	}

	mz_zip_reader_end(&zip);
	return false;
}

bool extract_zip(const std::filesystem::path& zip_path, const std::string& target_dir, const std::string& inner_folder = "")
{
	mz_zip_archive zip = {};
	if (!init_zip_from_file(zip_path, zip)) {
		return false;
	}

	bool result = true;
	mz_uint file_count = mz_zip_reader_get_num_files(&zip);
	
	for (mz_uint i = 0u; i < file_count; i++)
	{
		mz_zip_archive_file_stat stat;
		if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
			continue;
		}

		auto entry_path = std::filesystem::path(stat.m_filename);
		if (!inner_folder.empty())
		{
			std::filesystem::path inner(inner_folder);
			if (!entry_path.native().starts_with(inner.native())) {
				continue;
			}
			entry_path = entry_path.lexically_relative(inner);
		}

		if (stat.m_is_directory) {
			continue;
		}

		std::filesystem::path out_path = std::filesystem::path(target_dir) / entry_path;
		
		// Validate output path to prevent directory traversal
		std::filesystem::path canonical_target = std::filesystem::canonical(std::filesystem::path(target_dir));
		std::filesystem::path canonical_output = std::filesystem::absolute(out_path);

		if (!canonical_output.native().starts_with(canonical_target.native())) {
			continue; // Skip paths outside target directory
		}

		try {
			create_directories(out_path.parent_path());
		}
		catch (const std::exception&) 
		{
			log_error("Failed to create directory: " + out_path.parent_path().string());
			MessageBoxA(nullptr, ("Failed to create directory: " + out_path.parent_path().string()).c_str(), "Error", MB_ICONERROR);
			result = false;
			continue;
		}

		if (i > 0 && (i % 30 == 0)) {
			Sleep(10);
		}

		if (!mz_zip_reader_extract_to_file(&zip, i, out_path.string().c_str(), 0))
		{
			log_error("Failed to extract: " + std::string(stat.m_filename));
			MessageBoxA(nullptr, ("Failed to extract: " + std::string(stat.m_filename)).c_str(), "Error", MB_ICONERROR);
			result = false;
		}
	}

	mz_zip_reader_end(&zip);
	return result;
}

// 
// 

int main()
{
	std::cout << "\n" << std::setw(2);
	Sleep(200);
	
	
	log_default(true);
	std::cout << "Grand Theft Auto IV - RTX Remix Compatibility Mod Installer (github.com/xoxor4d/gta4-rtx)\n";
	log_default();
	std::cout << "- Make sure that you've placed both the installer and the 'GTAIV-Remix-CompatibilityMod-X.X.X.zip' into the same folder.\n";
	std::cout << "- If installation fails, refer to the release page on GitHub for manual install instructions.\n\n";

	std::cout << "Select your GTAIV.exe inside the GTAIV directory to continue ...\n";
	Sleep(500);

	// select GTAIV.exe
    std::string gtaiv_exe_path = open_file_dialog();
	if (gtaiv_exe_path.empty()) 
	{
		log_error("Invalid Path. Exiting ...");
		MessageBoxA(nullptr, "Something went wrong", "Error", MB_ICONERROR);
		return 0;
	}

    const std::string game_dir = std::filesystem::path(gtaiv_exe_path).parent_path().string();
	
	// Validate game directory exists
	if (!std::filesystem::exists(game_dir) || !std::filesystem::is_directory(game_dir)) 
	{
		MessageBoxA(nullptr, "Invalid game directory selected.", "Error", MB_ICONERROR);
		return 1;
	}
	
	std::cout << "> Using Path: '" << game_dir << "'\n\n";
	
	// Find zip file first (needed for version comparison)
	static const wchar_t* zip_prefix = L"GTAIV-Remix-CompatibilityMod";
	static const std::string zip_prefix_str = "GTAIV-Remix-CompatibilityMod";
	std::filesystem::path found_zip;

	auto installer_path = []()
		{
			wchar_t buf[MAX_PATH] = { 0 };
			GetModuleFileNameW(nullptr, buf, MAX_PATH);
			return std::filesystem::path(buf).parent_path();
		};

	// Collect all matching zip files with their versions
	std::vector<std::pair<std::filesystem::path, std::vector<int>>> zip_candidates;

	for (const auto& entry : std::filesystem::directory_iterator(installer_path()))
	{
		if (!entry.is_regular_file()) {
			continue;
		}

		const auto& p = entry.path();
		if (p.extension() == L".zip" && p.stem().wstring().starts_with(zip_prefix))
		{
			// Extract version from filename: GTAIV-Remix-CompatibilityMod-X.Y.Z.zip
			std::string stem = p.stem().string();
			std::string version_str;
			
			if (stem.length() > zip_prefix_str.length()) 
			{
				// Skip the prefix and any leading dash
				version_str = stem.substr(zip_prefix_str.length());
				if (!version_str.empty() && version_str[0] == '-') {
					version_str = version_str.substr(1);
				}
			}
			
			std::vector<int> version = parse_version(version_str);
			zip_candidates.push_back({p, version});
		}
	}

	// Find the zip with the highest version
	if (!zip_candidates.empty())
	{
		found_zip = zip_candidates[0].first;
		std::vector<int> best_version = zip_candidates[0].second;
		
		for (size_t i = 1; i < zip_candidates.size(); i++) 
		{
			if (version_greater_than(zip_candidates[i].second, best_version)) 
			{
				found_zip = zip_candidates[i].first;
				best_version = zip_candidates[i].second;
			}
		}
		
		std::cout << "> Found compatibility mod: " << found_zip.filename().string() << "\n";
	}

	bool skip_rtx_comp_install = false;
	if (found_zip.empty()) 
	{
		log_error("Could not find any zip starting with 'GTAIV-Remix-CompatibilityMod'.");
		std::cout << "Installation might be incomplete. Download and place the .zip next to the installer or install manually.\n";
		std::cout << "Checking for Remix Base Mod updates ...\n\n";
		skip_rtx_comp_install = true;
	}
	
	bool has_remix_comp_mod = false;

	if (!skip_rtx_comp_install)
	{
		// Validate zip file exists and is readable
		if (!std::filesystem::exists(found_zip) || !std::filesystem::is_regular_file(found_zip))
		{
			log_error("Found zip file but it is not accessible: " + found_zip.string());
			MessageBoxA(nullptr, "The zip file found is not accessible.", "Error", MB_ICONERROR);
			return 1;
		}

		std::cout << "Checking for FusionFix presence ...\n";
		Sleep(500);


		// check if any FusionFix version exists
		const bool has_fusion_fix = file_exists(game_dir + "\\d3d9.dll") &&
			file_exists(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi");

		// check if original FusionFix version exists
		const bool has_original_fusion_fix = has_fusion_fix && file_exists(game_dir + "\\vulkan.dll");

		// check if comp mod and remix are installed -> update
		has_remix_comp_mod = file_exists(game_dir + "\\d3d9.dll") &&
			file_exists(game_dir + "\\a_gta4-rtx.asi");

		// check if RTXRemix FusionFix fork marker exists
		const bool has_rtxremix_fusionfix_marker = file_exists(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.RTXRemix.txt");

		if (has_remix_comp_mod) {
			std::cout << "> Detected another version of the RTX Remix Compatibility Mod. Updating ... \n";
		}

		bool opt_install_fusion_fix_fork = false;

		// If RTXRemix FusionFix marker exists, compare versions
		if (has_rtxremix_fusionfix_marker)
		{
			// Read version from existing marker file
			std::string existing_version = read_file_from_disk(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.RTXRemix.txt");

			// Read version from zip file
			std::string zip_version = read_file_from_zip(found_zip, "_installer_options/FusionFix_RTXRemixFork/plugins/GTAIV.EFLC.FusionFix.RTXRemix.txt");

			// Trim whitespace from both versions
			auto trim = [](std::string& s)
				{
					s.erase(0, s.find_first_not_of(" \t\n\r"));
					s.erase(s.find_last_not_of(" \t\n\r") + 1);
				};

			trim(existing_version);
			trim(zip_version);

			if (existing_version == zip_version)
			{
				// Versions match, skip update
				opt_install_fusion_fix_fork = false;
				std::cout << "> RTXRemix FusionFix fork is up to date (version: " << existing_version << "). Skipping update.\n\n";
			}
			else
			{
				// Versions differ, ask user if they want to update
				/*std::string message = "A newer version of the RTXRemix FusionFix fork is available.\n\n";
				message += "Current version: " + (existing_version.empty() ? "Unknown" : existing_version) + "\n";
				message += "New version: " + (zip_version.empty() ? "Unknown" : zip_version) + "\n\n";
				message += "Do you want to update?";

				const auto res = MessageBoxA(nullptr, message.c_str(), "FusionFix Update", MB_YESNO | MB_ICONQUESTION);
				opt_install_fusion_fix_fork = (res == IDYES);*/
				opt_install_fusion_fix_fork = true; 
				//if (opt_install_fusion_fix_fork) {
					std::cout << "> Updating RTXRemix FusionFix fork from " << existing_version << " to " << zip_version << ".\n\n";
				//}
				/*else
				{
					log_yellow(true);
					std::cout << "> Skipping RTXRemix FusionFix fork update.\n\n";
					log_default();
				}*/
			}
		}
		else if (has_original_fusion_fix)
		{
			const auto res = MessageBoxA(nullptr, "FusionFix detected. Replace with a fork specifically tailored for RTX Remix? (Recommended)", "FusionFix", MB_YESNO | MB_ICONQUESTION);
			opt_install_fusion_fix_fork = (res == IDYES);

			if (!opt_install_fusion_fix_fork) {
				log_error("Not replacing installed FusionFix version. This might lead to issues.\n");
			} else {
				std::cout << "Installing RTXRemix FusionFix Fork.\n\n";
			}
		}
		else
		{
			// If user has FusionFix but not original FusionFix and no marker, they likely have RTXRemix fork already
			// Extract the marker file from the zip so future updates can detect it
			if (has_remix_comp_mod && !has_rtxremix_fusionfix_marker && !has_original_fusion_fix && has_fusion_fix)
			{
				// Ensure plugins directory exists
				const std::string plugins_dir = game_dir + "\\plugins";
				std::filesystem::create_directories(plugins_dir);

				// Extract the marker file from the zip
				const std::string marker_path = plugins_dir + "\\GTAIV.EFLC.FusionFix.RTXRemix.txt";
				if (extract_single_file_from_zip(found_zip, "_installer_options/FusionFix_RTXRemixFork/plugins/GTAIV.EFLC.FusionFix.RTXRemix.txt", marker_path))
				{
					log_blue(true);
					std::cout << "Extracted RTXRemix FusionFix marker file.\n\n";
					log_default();
				}
				else
				{
					log_yellow(true);
					std::cout << "[WARN] Failed to extract RTXRemix FusionFix marker file.\n\n";
					log_default();
				}

				opt_install_fusion_fix_fork = true;
			}

			if (!has_remix_comp_mod || !has_fusion_fix)
			{
				const auto res = MessageBoxA(nullptr, "Install FusionFix fork specifically tailored for RTX Remix? (Recommended)", "FusionFix", MB_YESNO | MB_ICONQUESTION);
				opt_install_fusion_fix_fork = res == IDYES;
				std::cout << (opt_install_fusion_fix_fork ? "Installing FusionFix RTXRemix Fork." : "Not installing FusionFix RTXRemix Fork.") << "\n\n";
			}
		}

		// ask for fullscreen / windowed
		bool fullscreen = true;

		// Only ask about display mode and Steam args if this is a fresh install (a_gta4-rtx.asi doesn't exist)
		if (!has_remix_comp_mod)
		{
			if (const auto res = MessageBoxA(nullptr, "Setup GTA IV to run in fullscreen/borderless mode?\n(Choose No if you want to run the game in windowed mode)", "Display mode", MB_YESNO | MB_ICONQUESTION))
			{
				if (res == IDNO) {
					fullscreen = false;
				} else {
					std::cout << "If you are having trouble with launching the game in fullscreen:\n> Go into 'rtx_comp/game_settings.toml'\n> Set 'manual_game_resolution_enabled' to 'true'\n> Set your desired resolution via 'manual_game_resolution'\n\n";
				}
			}

			// steam launch args warning
			MessageBoxA(nullptr, "If you run the game from Steam, make sure to remove ALL launch arguments from Steam properties for GTA IV!\n", "IMPORTANT", MB_OK | MB_ICONWARNING);
		}

		// backup some files

		if (MoveFileExA(
			(game_dir + "\\rtx_comp\\comp_settings.toml").c_str(),
			(game_dir + "\\rtx_comp\\comp_settings.toml.bak").c_str(),
			MOVEFILE_REPLACE_EXISTING))
		{
			std::cout << "Renamed 'comp_settings.toml' to 'comp_settings.toml.bak'\n";
		}
		Sleep(25);

		if (MoveFileExA(
			(game_dir + "\\rtx.conf").c_str(),
			(game_dir + "\\rtx.conf.bak").c_str(), MOVEFILE_REPLACE_EXISTING))
		{
			std::cout << "Renamed 'rtx.conf' to 'rtx.conf.bak'\n";
		}
		Sleep(25);

		// extract comp files

		log_yellow(true);
		std::cout << "Extracting zip ...\n";
		log_default();

		Sleep(100); // Small delay before extraction

		if (!extract_zip(found_zip, game_dir, "GTAIV-Remix-CompatibilityMod"))
		{
			log_error(
				"Failed to extract 'GTAIV-Remix-CompatibilityMod' files from 'GTAIV-Remix-CompatibilityMod.zip'\n"
				"> Please extract files manually.");

			MessageBoxA(nullptr, "Something went wrong.\nCheck console.", "Error", MB_ICONERROR);
			//return 0;
		}

		std::cout << "> Done!\n";

		Sleep(100); // Small delay between extractions

		if (!has_remix_comp_mod)
		{
			// extract fullscreen or windowed files
			std::string windowed_or_fullscreen_path = fullscreen ? "_installer_options/mode_fullscreen/" : "_installer_options/mode_windowed/";
			if (!extract_zip(found_zip, game_dir, windowed_or_fullscreen_path)) {
				log_error("Failed to extract '" + windowed_or_fullscreen_path + "' files from 'GTAIV-Remix-CompatibilityMod.zip'");
			}

			Sleep(100); // Small delay before next operation
		}

		// install FusionFix fork if requested
		if (opt_install_fusion_fix_fork)
		{
			Sleep(100); // Small delay before FusionFix installation
			if (has_original_fusion_fix)
			{
				if (MoveFileExA(
					(game_dir + "\\update").c_str(),
					(game_dir + "\\update_originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << "Renamed 'update' folder to 'update_originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\vulkan.dll").c_str(),
					(game_dir + "\\vulkan.dll.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << "Renamed 'vulkan.dll' to 'vulkan.dll.originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi").c_str(),
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.asi.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << "Renamed 'GTAIV.EFLC.FusionFix.asi' to 'GTAIV.EFLC.FusionFix.asi.originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.cfg").c_str(),
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.cfg.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << "Renamed 'GTAIV.EFLC.FusionFix.cfg' to 'GTAIV.EFLC.FusionFix.cfg.originalFF'\n";
				}
				Sleep(25);

				if (MoveFileExA(
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.ini").c_str(),
					(game_dir + "\\plugins\\GTAIV.EFLC.FusionFix.ini.originalFF").c_str(),
					MOVEFILE_REPLACE_EXISTING))
				{
					std::cout << "Renamed 'GTAIV.EFLC.FusionFix.ini' to 'GTAIV.EFLC.FusionFix.ini.originalFF'\n";
				}
				Sleep(25);
			}

			if (!extract_zip(found_zip, game_dir, "_installer_options/FusionFix_RTXRemixFork/")) {
				log_error("Failed to extract '_installer_options/FusionFix_RTXRemixFork/' files from 'GTAIV-Remix-CompatibilityMod.zip'");
			}

			std::cout << "> Extracted FusionFix RTX Remix Fork\n\n";
		}
	}

	// --------------
	// Base Remix Mod

	{
		static const char* base_mod_repo_owner = "xoxor4d";
		static const char* base_mod_repo_name = "gta4-rtx-base-mod";
		static const char* base_mod_branch = "master";
		static const char* base_mod_repo_url = "https://github.com/xoxor4d/gta4-rtx-base-mod";
		static const char* base_mod_zip_url = "https://github.com/xoxor4d/gta4-rtx-base-mod/archive/refs/heads/master.zip";
		static const char* base_mod_zip_inner_mods_github = "gta4-rtx-base-mod-master/mods";
		static const char* base_mod_zip_inner_mods_flat = "mods";
		
		auto get_installer_dir = []()
		{
			wchar_t buf[MAX_PATH] = { 0 };
			GetModuleFileNameW(nullptr, buf, MAX_PATH);
			return std::filesystem::path(buf).parent_path();
		};
		
		const std::filesystem::path mods_dir = std::filesystem::path(game_dir) / "rtx-remix" / "mods";
		const std::filesystem::path commit_file = mods_dir / "gta4rtx_commit.txt";
		const std::string commit_file_str = commit_file.string();
		
		// Ensure mods directory exists
		std::filesystem::create_directories(mods_dir);

		// Helper function to download and extract base mod with SHA-based naming
		auto download_and_extract_base_mod = [&](const std::string& sha) -> bool
		{
			const std::string short_sha = get_short_sha(sha);
			const std::filesystem::path base_zip_path = get_installer_dir() / ("gta4-rtx-base-mod-" + short_sha + ".zip");

			// Download the zip
			std::cout << "\nDownloading base mod zip to:\n> " << base_zip_path.string() << "\n\n";

			// Convert char* to wstring
			std::string zip_url_str(base_mod_zip_url);
			std::wstring zip_url_w(zip_url_str.begin(), zip_url_str.end());

			if (!download_file_to_path(zip_url_w, base_zip_path))
			{
				log_error(
					"Download failed.\n"
					"Manually download from:\n" + std::string(base_mod_zip_url) + "\n"
					"Or from a mirror found in the release notes on GitHub.\n\n"
					"Extract contents to:\n" + mods_dir.string());
				
				MessageBoxA(nullptr,
							"Failed to download base remix-mod.\n\n"
							"Please try again or proceed manually.\n"
							"Check the console for more details.",
							"Error",
							MB_ICONERROR);

				return false;
			}
			
			// Extract the mods folder
			log_yellow(true);
			std::cout << "\nExtracting base mod into rtx-remix/mods ...\n";
			log_default();
			bool ok_extract = extract_zip(base_zip_path, mods_dir.string(), base_mod_zip_inner_mods_github);

			if (!ok_extract) { // Fallback for archives that have 'mods/...' at the root
				ok_extract = extract_zip(base_zip_path, mods_dir.string(), base_mod_zip_inner_mods_flat);
			}
			
			if (!ok_extract) 
			{
				log_error(
					"Failed to extract base remix-mod.\n"
					"You can extract it manually from:\n" + base_zip_path.string() + "\n\n"
					"Extract contents to:\n" + mods_dir.string());

				MessageBoxA(nullptr,
							"[!] Failed to extract base remix-mod.\n\nCheck the console for more details.\n",
							"Error",
							MB_ICONERROR);

				return false;
			}

			return true;
		};

		// Check if commit file exists
		const bool commit_file_exists = std::filesystem::exists(commit_file) && std::filesystem::is_regular_file(commit_file);
		if (!commit_file_exists) 
		{
			log_blue(true);
			std::cout << "\n\nRequired: Download and extract the base remix-mod? You can skip this if you installed it manually.\n";
			log_default();

			// Print full info (including links) to console so the user can copy them
			std::cout
				<< "This contains actual remix replacements such as PBR textures, mesh fixes etc.\n\n"
				<< "Direct zip link:\n> " << base_mod_zip_url << "\n\n"
				<< "Repo:\n> " << base_mod_repo_url << "\n\n"
				<< "This will place the downloaded zip next to the installer, then extract the 'mods' folder into:\n> "
				<< mods_dir.string() << "\n";

			const int user_choice = MessageBoxA(nullptr, "Required: Download and extract the base remix-mod?", "Base Remix-Mod", MB_YESNO | MB_ICONQUESTION);
			
			if (user_choice != IDYES) 
			{
				MessageBoxA(nullptr,
							"The base-remix-mod is required for the game to function properly.\n\n"
							"Make sure to install it via the installer or manually.",
							"(See release notes on GitHub)",
							MB_OK);

				//return 0;
			}
			else
			{
				// Fetch SHA first so we can name the zip file
				std::cout << "Fetching latest commit SHA from GitHub...\n";
				const auto latest_sha = get_latest_github_commit_sha(base_mod_repo_owner, base_mod_repo_name, base_mod_branch);

				if (latest_sha.empty())
				{
					log_error("Failed to fetch commit SHA from GitHub. Network error or API failure.");
					MessageBoxA(nullptr, "Failed to fetch commit information from GitHub.\nPlease check your internet connection.", "Error", MB_ICONERROR);
					return 0;
				}

				// Download and extract
				if (!download_and_extract_base_mod(latest_sha)) {
					return 0;
				}

				std::cout << "> Done!\n\n";

				// Save the commit SHA
				if (write_file_to_disk(commit_file_str, latest_sha))
				{
					std::cout << "Saved commit SHA: " << latest_sha << "\n";
					std::cout << "Commit file saved to: " << commit_file_str << "\n";
				}
				else
				{
					log_yellow(true);
					std::cout << "[WARN] Failed to save commit file to: " << commit_file_str << "\n";
					log_default();
				}
			}
		} 
		else // commit_file_exists
		{
			// File exists, compare with GitHub
			std::cout << "\nChecking latest gta4-rtx-base-mod commit SHA...\n";
			int comparison = compare_commit_sha(commit_file_str, base_mod_repo_owner, base_mod_repo_name, base_mod_branch);
			
			if (comparison == 0) {
				std::cout << "Installed base-mod matches GitHub (up to date).\n";
			} 
			else if (comparison == 1) 
			{
				std::string local_sha = trim_whitespace(read_file_from_disk(commit_file_str));
				std::string latest_sha = get_latest_github_commit_sha(base_mod_repo_owner, base_mod_repo_name, base_mod_branch);
				
				if (!latest_sha.empty()) 
				{
					std::cout
						<< "\nA newer version of the gta4-rtx-base-mod is available on GitHub.\n"
						<< "Current commit: " + (local_sha.empty() ? "Unknown" : local_sha) + "\n"
						<< "Latest commit: " + latest_sha + "\n"
						<< "Repo: " + std::string(base_mod_repo_url) + "\n\n";

					const int user_choice = MessageBoxA(nullptr, "A newer version of the base mod is available on GitHub.\nWould you like to update?", "Base Mod Update Available", MB_YESNO | MB_ICONQUESTION);
					if (user_choice == IDYES) 
					{
						// Download and extract the update
						if (download_and_extract_base_mod(latest_sha))
						{
							// Only update commit file after successful download and extraction
							if (write_file_to_disk(commit_file_str, latest_sha)) {
								std::cout << "Updated commit SHA to: " << latest_sha << "\n\n";
							} else 
								{
								log_yellow(true);
								std::cout << "[WARN] Failed to update commit file.\n\n";
								log_default();
							}
						}
					} else {
						std::cout << "Skipping base mod update.\n\n";
					}
				} 
				else 
				{
					log_yellow(true);
					std::cout << "[WARN] Could not fetch latest commit SHA for comparison.\n\n";
					log_default();
				}
			} 
			else if (comparison == -2) 
			{
				log_yellow(true);
				std::cout << "[WARN] Could not check commit SHA (network error or file issue).\n\n";
				log_default();
			}
		}
	}

	// --------------
	// AutoPBR Remix Mod (Optional)

	{
		static const char* autopbr_mod_repo_owner = "xoxor4d";
		static const char* autopbr_mod_repo_name = "gta4-rtx-autopbr-mod";
		static const char* autopbr_mod_branch = "master";
		static const char* autopbr_mod_repo_url = "https://github.com/xoxor4d/gta4-rtx-autopbr-mod";
		static const char* autopbr_mod_zip_url = "https://github.com/xoxor4d/gta4-rtx-autopbr-mod/archive/refs/heads/master.zip";
		static const char* autopbr_mod_zip_inner_mods_github = "gta4-rtx-autopbr-mod-master/mods";
		static const char* autopbr_mod_zip_inner_mods_flat = "mods";
		
		auto get_installer_dir = []()
		{
			wchar_t buf[MAX_PATH] = { 0 };
			GetModuleFileNameW(nullptr, buf, MAX_PATH);
			return std::filesystem::path(buf).parent_path();
		};
		
		const std::filesystem::path mods_dir = std::filesystem::path(game_dir) / "rtx-remix" / "mods";
		const std::filesystem::path commit_file = mods_dir / "gta4rtx_autopbr_commit.txt";
		const std::string commit_file_str = commit_file.string();
		
		// Ensure mods directory exists
		std::filesystem::create_directories(mods_dir);

		// Helper function to download and extract autopbr mod with SHA-based naming
		auto download_and_extract_autopbr_mod = [&](const std::string& sha) -> bool
		{
			const std::string short_sha = get_short_sha(sha);
			const std::filesystem::path autopbr_zip_path = get_installer_dir() / ("gta4-rtx-autopbr-mod-" + short_sha + ".zip");

			// Download the zip
			std::cout << "\nDownloading AutoPBR mod zip to:\n> " << autopbr_zip_path.string() << "\n\n";

			// Convert char* to wstring
			std::string zip_url_str(autopbr_mod_zip_url);
			std::wstring zip_url_w(zip_url_str.begin(), zip_url_str.end());

			if (!download_file_to_path(zip_url_w, autopbr_zip_path))
			{
				log_error(
					"Download failed.\n"
					"Manually download:\n" + std::string(autopbr_mod_zip_url) + "\n"
					"Or from a mirror found in the release notes on GitHub.\n\n"
					"Extract contents to:\n" + mods_dir.string());
				
				MessageBoxA(nullptr,
							"Failed to download AutoPBR remix-mod.\n\n"
							"Please try again or proceed manually.\n"
							"Check the console for more details.",
							"Error",
							MB_ICONERROR);

				return false;
			}
			
			// Extract the mods folder
			log_yellow(true);
			std::cout << "\nExtracting AutoPBR mod into rtx-remix/mods ...\n";
			log_default();
			bool ok_extract = extract_zip(autopbr_zip_path, mods_dir.string(), autopbr_mod_zip_inner_mods_github);

			if (!ok_extract) { // Fallback for archives that have 'mods/...' at the root
				ok_extract = extract_zip(autopbr_zip_path, mods_dir.string(), autopbr_mod_zip_inner_mods_flat);
			}
			
			if (!ok_extract) 
			{
				log_error(
					"Failed to extract AutoPBR remix-mod.\n"
					"You can extract it manually from:\n" + autopbr_zip_path.string() + "\n\n"
					"Extract contents to:\n" + mods_dir.string());

				MessageBoxA(nullptr,
							"[!] Failed to extract AutoPBR remix-mod.\n\nCheck the console for more details.\n",
							"Error",
							MB_ICONERROR);

				return false;
			}

			return true;
		};

		// Check if commit file exists
		const bool commit_file_exists = std::filesystem::exists(commit_file) && std::filesystem::is_regular_file(commit_file);
		if (!commit_file_exists) 
		{
			log_blue(true);
			std::cout << "\n\nOptional: Download and extract the AutoPBR remix-mod?\n";
			log_default();

			// Print full info (including links) to console so the user can copy them
			std::cout
				<< "This contains automatically converted PBR materials for textures.\n"
				<< "Download size: ~1.5 GB\n\n"
				<< "Direct zip link:\n> " << autopbr_mod_zip_url << "\n\n"
				<< "Repo:\n> " << autopbr_mod_repo_url << "\n\n"
				<< "This will place the downloaded zip next to the installer, then extract the 'mods' folder into:\n> "
				<< mods_dir.string() << "\n";

			const int user_choice = MessageBoxA(nullptr, 
				"Optional: Download and extract the AutoPBR remix-mod?\n\n"
				"This contains automatically converted PBR materials for textures.\n\n"
				"Download size: ~1.5 GB", 
				"AutoPBR Remix-Mod", MB_YESNO | MB_ICONQUESTION);
			
			if (user_choice == IDYES)
			{
				// Fetch SHA first so we can name the zip file
				std::cout << "Fetching latest commit SHA from GitHub...\n";
				const auto latest_sha = get_latest_github_commit_sha(autopbr_mod_repo_owner, autopbr_mod_repo_name, autopbr_mod_branch);
				
				if (latest_sha.empty()) 
				{
					log_yellow(true);
					std::cout << "[WARN] Failed to fetch commit SHA from GitHub. Network error or API failure.\n";
					std::cout << "Skipping AutoPBR mod installation.\n\n";
					log_default();
				}
				else
				{
					// Download and extract
					if (download_and_extract_autopbr_mod(latest_sha))
					{
						std::cout << "> Done!\n\n";

						// Save the commit SHA
						if (write_file_to_disk(commit_file_str, latest_sha)) 
						{
							std::cout << "Saved commit SHA: " << latest_sha << "\n";
							std::cout << "Commit file saved to: " << commit_file_str << "\n";
						} 
						else 
						{
							log_yellow(true);
							std::cout << "[WARN] Failed to save commit file to: " << commit_file_str << "\n";
							log_default();
						}
					}
				}
			}
			else
			{
				std::cout << "Skipping AutoPBR mod installation.\n\n";
			}
		} 
		else // commit_file_exists
		{
			// File exists, compare with GitHub
			std::cout << "\nChecking latest gta4-rtx-autopbr-mod commit SHA...\n";
			int comparison = compare_commit_sha(commit_file_str, autopbr_mod_repo_owner, autopbr_mod_repo_name, autopbr_mod_branch);
			
			if (comparison == 0) {
				std::cout << "Installed AutoPBR mod matches GitHub (up to date).\n";
			} 
			else if (comparison == 1) 
			{
				std::string local_sha = trim_whitespace(read_file_from_disk(commit_file_str));
				std::string latest_sha = get_latest_github_commit_sha(autopbr_mod_repo_owner, autopbr_mod_repo_name, autopbr_mod_branch);
				
				if (!latest_sha.empty()) 
				{
					std::cout
						<< "\nA newer version of the gta4-rtx-autopbr-mod is available on GitHub.\n"
						<< "Current commit: " + (local_sha.empty() ? "Unknown" : local_sha) + "\n"
						<< "Latest commit: " + latest_sha + "\n"
						<< "Repo: " + std::string(autopbr_mod_repo_url) + "\n\n";

					const int user_choice = MessageBoxA(nullptr, 
						"A newer version of the AutoPBR mod is available on GitHub.\n\n"
						"Download size: ~1.8 GB\n\n"
						"Would you like to update?", 
						"AutoPBR Mod Update Available", MB_YESNO | MB_ICONQUESTION);

					if (user_choice == IDYES) 
					{
						// Download and extract the update
						if (download_and_extract_autopbr_mod(latest_sha))
						{
							// Only update commit file after successful download and extraction
							if (write_file_to_disk(commit_file_str, latest_sha)) {
								std::cout << "Updated commit SHA to: " << latest_sha << "\n\n";
							} else 
							{
								log_yellow(true);
								std::cout << "[WARN] Failed to update commit file.\n\n";
								log_default();
							}
						}
					} 
					else {
						std::cout << "Skipping AutoPBR mod update.\n\n";
					}
				} 
				else 
				{
					log_yellow(true);
					std::cout << "[WARN] Could not fetch latest commit SHA for comparison.\n\n";
					log_default();
				}
			} 
			else if (comparison == -2) 
			{
				log_yellow(true);
				std::cout << "[WARN] Could not check AutoPBR commit SHA (network error or file issue).\n\n";
				log_default();
			}
		}
	}

	// Only prompt about DirectX if this is a fresh install (a_gta4-rtx.asi doesn't exist)
	if (!skip_rtx_comp_install && !has_remix_comp_mod)
	{
		// DX9 June 2010 runtime
		if (MessageBoxA(nullptr, "It's recommended to install Microsoft DirectX June 2010 Redistributable.\nDo you want to open a link to the installer?\n(https://www.microsoft.com/en-us/download/details.aspx?id=8109)", "DirectX Runtime", MB_YESNO | MB_ICONQUESTION) == IDYES) {
			ShellExecuteA(nullptr, "open", "https://www.microsoft.com/en-us/download/details.aspx?id=8109", nullptr, nullptr, SW_SHOWNORMAL);
		}
	}

	log_green(true);
	std::cout << 
		"\nDone! - If you run into issues, visit: https://github.com/xoxor4d/gta4-rtx/wiki/Troubleshooting---Guides or create an issue on the GitHub repository.\n"
		"> Please include the external console log (rtx_comp/logfile.txt)\n"
		"> The log files from 'rtx-remix/logs'\n"
		"> A short description and anything else that might help to identify the issue.\n";
	log_default();

	MessageBoxA(nullptr, "Installation complete!\nYou can now launch GTA IV.", "Success", MB_ICONINFORMATION);
    return 0;
}