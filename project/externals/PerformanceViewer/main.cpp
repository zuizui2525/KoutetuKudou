#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <format>
#include <sstream>
#include <algorithm>
#include <cstdint>

#include "../../Engine/Debug/PerformanceReporter/SharedPerfData.h"

// Suppress console window and run as windows application with 'main' entry
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#pragma comment(lib, "ws2_32.lib")



namespace fs = std::filesystem;

namespace {
    constexpr int kDefaultPort = 8080;
    constexpr size_t kMaxPathLen = 260;
    int s_currentPort = kDefaultPort;
    std::string s_reportPath = "";



#ifdef _DEBUG
    std::string s_buildMode = "Debug";
#elif defined(DEVELOPMENT)
    std::string s_buildMode = "Development";
#else
    std::string s_buildMode = "Release";
#endif

    // 探索するレポートディレクトリのデフォルト名
    const std::string kTargetReportDir = "out/performance_reports";

    struct LiveMetricEntry {
        float time;
        float fps;
        float cpu_mb;
        float vram_mb;
    };
    std::vector<LiveMetricEntry> s_liveMetrics;
    constexpr size_t kMaxLiveMetricsSize = 120; // 300ms * 120 = 36 seconds

    // Find the latest report directory
    std::string FindLatestReport() {
        std::string searchPaths[] = {
            kTargetReportDir,
            "../" + kTargetReportDir,
            "../../" + kTargetReportDir,
            "externals/PerformanceViewer/" + kTargetReportDir
        };
        std::string foundTarget = "";
        std::error_code ec;
        for (const auto& path : searchPaths) {
            if (fs::exists(path, ec)) {
                foundTarget = path;
                break;
            }
        }

        if (foundTarget.empty()) {
            return "";
        }

        fs::path latestPath;
        std::time_t latestTime = 0;

        for (auto it = fs::directory_iterator(foundTarget, ec); it != fs::directory_iterator(); it.increment(ec)) {
            if (ec) break;
            if (it->is_directory(ec)) {
                for (auto subIt = fs::directory_iterator(it->path(), ec); subIt != fs::directory_iterator(); subIt.increment(ec)) {
                    if (ec) break;
                    if (subIt->is_directory(ec) && subIt->path().filename().string().find("report_") == 0) {
                        auto writeTime = subIt->last_write_time(ec);
                        if (!ec) {
                            auto sct = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                                writeTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                            std::time_t t = std::chrono::system_clock::to_time_t(sct);

                            if (t > latestTime) {
                                latestTime = t;
                                latestPath = subIt->path();
                            }
                        }
                    }
                }
            }
        }
        return latestPath.string();
    }


    std::string GetMimeType(const std::string& extension) {
        if (extension == ".html") return "text/html; charset=utf-8";
        if (extension == ".css") return "text/css; charset=utf-8";
        if (extension == ".js") return "application/javascript; charset=utf-8";
        if (extension == ".json") return "application/json; charset=utf-8";
        if (extension == ".bmp") return "image/bmp";
        if (extension == ".png") return "image/png";
        if (extension == ".mp4") return "video/mp4";
        if (extension == ".md") return "text/markdown; charset=utf-8";
        return "application/octet-stream";
    }

    // Helper to send file content to client
    void SendFile(SOCKET clientSocket, const std::string& filePath, const std::string& mimeType) {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(clientSocket, response.c_str(), (int)response.size(), 0);
            return;
        }

        // Get file size
        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        // Send header
        std::string header = std::format(
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: {}\r\n"
            "Content-Length: {}\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Connection: close\r\n\r\n",
            mimeType, fileSize
        );
        send(clientSocket, header.c_str(), (int)header.size(), 0);

        // Send data chunks
        std::vector<char> buffer(4096);
        while (file.read(buffer.data(), buffer.size()) || file.gcount() > 0) {
            send(clientSocket, buffer.data(), (int)file.gcount(), 0);
        }
    }
}

int main(int argc, char* argv[]) {
    // 1. 最優先で WinSock を初期化
    static WSADATA s_wsaData;
    std::memset(&s_wsaData, 0, sizeof(s_wsaData));
    if (WSAStartup(MAKEWORD(2, 2), &s_wsaData) != 0) {
        std::cerr << "[ERROR] WSAStartup failed!" << std::endl;
        return 1;
    }

    /* 起動直後に前回の消し残しがあればクリーンアップを実行する */
    char currentExePath[kMaxPathLen];
    if (GetModuleFileNameA(NULL, currentExePath, static_cast<DWORD>(kMaxPathLen)) > 0) {
        fs::path oldFile = fs::path(currentExePath).parent_path() / "PerformanceViewer.exe.old";
        if (fs::exists(oldFile)) {
            std::error_code ec;
            fs::remove(oldFile, ec);
        }
    }

    // Determine report folder
    if (argc > 1) {
        s_reportPath = argv[1];
    } else {
        s_reportPath = FindLatestReport();
    }

    SOCKET listenSocket = INVALID_SOCKET;
    int port = kDefaultPort;
    bool bound = false;

    while (!bound && port < kDefaultPort + 100) { // 最大100個のポートを試す
        // AF_INET / SOCK_STREAM / IPPROTO_TCP または 0 でフォールバック
        listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listenSocket == INVALID_SOCKET) {
            listenSocket = socket(AF_INET, SOCK_STREAM, 0);
        }
        if (listenSocket == INVALID_SOCKET) {
            listenSocket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
        }


        std::cout << "[DEBUG] Created socket: " << listenSocket << " (INVALID=" << INVALID_SOCKET << ")" << std::endl;
        if (listenSocket == INVALID_SOCKET) {
            std::cerr << "[ERROR] socket creation failed! Error code: " << WSAGetLastError() << std::endl;
            WSACleanup();
            return 1;
        }







        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);

        /* Bind port */
        if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) != SOCKET_ERROR) {
            bound = true;
            s_currentPort = port;
        } else {
            closesocket(listenSocket);
            
            /* 初回ポート (8080) が使用中の場合のみ、既存のPerformanceViewerプロセスであるかを確認 */
            if (port == kDefaultPort) {
                SOCKET checkSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (checkSock != INVALID_SOCKET) {
                    sockaddr_in checkAddr{};
                    checkAddr.sin_family = AF_INET;
                    checkAddr.sin_port = htons(kDefaultPort);
                    inet_pton(AF_INET, "127.0.0.1", &checkAddr.sin_addr);

                    /* 100ms タイムアウト設定 */
                    DWORD dwTimeout = 100;
                    setsockopt(checkSock, SOL_SOCKET, SO_SNDTIMEO, (char*)&dwTimeout, sizeof(dwTimeout));
                    setsockopt(checkSock, SOL_SOCKET, SO_RCVTIMEO, (char*)&dwTimeout, sizeof(dwTimeout));

                    if (connect(checkSock, reinterpret_cast<sockaddr*>(&checkAddr), sizeof(checkAddr)) != SOCKET_ERROR) {
                        /* 既存のプロセスにヘルスチェックリクエストを送信 */
                        std::string req = "GET /check_viewer HTTP/1.1\r\nHost: localhost:8080\r\nConnection: close\r\n\r\n";
                        send(checkSock, req.c_str(), (int)req.size(), 0);

                        std::vector<char> respBuf(1024, 0);
                        int bytes = recv(checkSock, respBuf.data(), (int)respBuf.size() - 1, 0);
                        if (bytes > 0) {
                            std::string resp(respBuf.data());
                            if (resp.find("PerformanceViewer") != std::string::npos) {
                                closesocket(checkSock);

                                /* ビルドモードを新しいモードに更新するリクエストを送信 */
                                SOCKET updateSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                                if (updateSock != INVALID_SOCKET) {
                                    if (connect(updateSock, reinterpret_cast<sockaddr*>(&checkAddr), sizeof(checkAddr)) != SOCKET_ERROR) {
                                        std::string updateReq = std::format(
                                            "GET /update_build_mode?mode={} HTTP/1.1\r\n"
                                            "Host: localhost:8080\r\n"
                                            "Connection: close\r\n\r\n",
                                            s_buildMode
                                        );
                                        send(updateSock, updateReq.c_str(), (int)updateReq.size(), 0);
                                        char dummy[64];
                                        recv(updateSock, dummy, sizeof(dummy), 0);
                                    }
                                    closesocket(updateSock);
                                }

                                /* ブラウザで既存サーバーのURLを開き、自分自身は正常終了 */
                                std::string launchCmd = std::format("/c start http://localhost:{}/", kDefaultPort);
                                ShellExecuteA(NULL, "open", "cmd.exe", launchCmd.c_str(), NULL, SW_HIDE);

                                WSACleanup();
                                return 0;
                            }
                        }
                    }
                    closesocket(checkSock);
                }
            }

            /* ポートを+1して再試行 */
            port++;
        }
    }


    if (!bound) {
        std::cerr << "[ERROR] Could not bind to any port from " << kDefaultPort << " to " << (kDefaultPort + 100) << std::endl;
        WSACleanup();
        return 1;
    }

    /* Start listening */
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[ERROR] listen failed!" << std::endl;
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    /* Launch browser automatically via OS start command */
    std::string launchCmd = std::format("/c start http://localhost:{}/", s_currentPort);
    ShellExecuteA(NULL, "open", "cmd.exe", launchCmd.c_str(), NULL, SW_HIDE);

    std::cout << "[INFO] PerformanceViewer server started on port " << s_currentPort 
              << " (Mode: " << s_buildMode << ")" << std::endl;


    /* HTTP Request Loop */
    bool running = true;
    while (running) {

        SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            break;
        }

        // Read request header
        std::vector<char> reqBuffer(2048, 0);
        int bytesReceived = recv(clientSocket, reqBuffer.data(), (int)reqBuffer.size() - 1, 0);
        if (bytesReceived > 0) {
            std::string request(reqBuffer.data());
            std::stringstream ss(request);
            std::string method, rawPath;
            ss >> method >> rawPath;

            // Shutdown API (beforeunload時の sendBeacon は POST を投げるため、GET判定の外側で処理する)
            if (rawPath.find("/exit") == 0) {
                std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(clientSocket, response.c_str(), (int)response.size(), 0);
                closesocket(clientSocket);
                running = false;

                /* 自身の実行ファイルパスから .old の絶対パスを作成し、遅延クリーンアップを実行 */
                char exePath[kMaxPathLen];

                GetModuleFileNameA(NULL, exePath, static_cast<DWORD>(kMaxPathLen));
                fs::path oldPath = fs::path(exePath).parent_path() / "PerformanceViewer.exe.old";


                std::string cleanupCmd = std::format("/c timeout /t 3 /nobreak & del /F /Q \"{}\" 2>NUL", oldPath.string());
                ShellExecuteA(NULL, "open", "cmd.exe", cleanupCmd.c_str(), NULL, SW_HIDE);

                break;
            }

            if (method == "GET") {
                // Normalize path
                if (rawPath == "/") {
                    rawPath = "/index.html";
                }

                // Update Report Path API: GET /update_path?path=...
                if (rawPath.find("/update_path") == 0) {
                    size_t queryPos = rawPath.find("?path=");
                    if (queryPos != std::string::npos) {
                        std::string newPath = rawPath.substr(queryPos + 6);
                        
                        // Strip any remaining query params in the path variable
                        size_t ampPos = newPath.find("&");
                        if (ampPos != std::string::npos) {
                            newPath = newPath.substr(0, ampPos);
                        }

                        // Simple url-decode for path separators
                        size_t replacePos;
                        while ((replacePos = newPath.find("%2F")) != std::string::npos) {
                            newPath.replace(replacePos, 3, "/");
                        }
                        while ((replacePos = newPath.find("%2f")) != std::string::npos) {
                            newPath.replace(replacePos, 3, "/");
                        }
                        
                        s_reportPath = newPath;
                    }
                    std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    continue;
                }

                // Get Latest Report Path API: GET /get_latest_path (Prefix match to ignore ?t=...)
                if (rawPath.find("/get_latest_path") == 0) {
                    std::string cleanPath = s_reportPath;
                    size_t slashPos = 0;
                    while ((slashPos = cleanPath.find("\\", slashPos)) != std::string::npos) {
                        cleanPath.replace(slashPos, 1, "/");
                        slashPos += 1;
                    }
                    
                    std::string jsonResponse = std::format("{{\"latest_path\": \"{}\", \"build_mode\": \"{}\"}}", cleanPath, s_buildMode);
                    std::string response = std::format(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json; charset=utf-8\r\n"
                        "Content-Length: {}\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n{}",
                        jsonResponse.size(), jsonResponse
                    );
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    continue;
                }

                // Check Viewer API: GET /check_viewer
                if (rawPath.find("/check_viewer") == 0) {
                    std::string jsonResponse = "{\"app\": \"PerformanceViewer\"}";
                    std::string response = std::format(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json; charset=utf-8\r\n"
                        "Content-Length: {}\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n{}",
                        jsonResponse.size(), jsonResponse
                    );
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    continue;
                }

                // Update Build Mode API: GET /update_build_mode?mode=...
                if (rawPath.find("/update_build_mode") == 0) {
                    size_t queryPos = rawPath.find("?mode=");
                    if (queryPos != std::string::npos) {
                        std::string newMode = rawPath.substr(queryPos + 6);
                        size_t ampPos = newMode.find("&");
                        if (ampPos != std::string::npos) {
                            newMode = newMode.substr(0, ampPos);
                        }
                        s_buildMode = newMode;
                        std::cout << "[INFO] Build mode updated to: " << s_buildMode << std::endl;
                    }
                    std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    continue;
                }

                // Update Live Metrics API: GET /update_live_metrics?fps=...&cpu=...&vram=...&time=...
                if (rawPath.find("/update_live_metrics") == 0) {
                    auto ExtractParam = [](const std::string& url, const std::string& param) -> float {
                        size_t pos = url.find(param + "=");
                        if (pos == std::string::npos) return 0.0f;
                        size_t endPos = url.find("&", pos);
                        std::string valStr = url.substr(pos + param.size() + 1, endPos - (pos + param.size() + 1));
                        try {
                            return std::stof(valStr);
                        } catch (...) {
                            return 0.0f;
                        }
                    };

                    LiveMetricEntry entry;
                    entry.fps = ExtractParam(rawPath, "fps");
                    entry.cpu_mb = ExtractParam(rawPath, "cpu");
                    entry.vram_mb = ExtractParam(rawPath, "vram");
                    entry.time = ExtractParam(rawPath, "time");

                    s_liveMetrics.push_back(entry);
                    if (s_liveMetrics.size() > kMaxLiveMetricsSize) {
                        s_liveMetrics.erase(s_liveMetrics.begin());
                    }

                    std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    continue;
                }

                // Get Live Metrics API: GET /get_live_metrics (Prefix match to ignore ?t=...)
                if (rawPath.find("/get_live_metrics") == 0) {
                    // 共有メモリからゲームの最新パフォーマンス情報を直接読み取り
                    HANDLE hMap = OpenFileMappingA(FILE_MAP_READ, FALSE, ZuizuiPerf::kSharedMemoryName);
                    if (hMap != NULL) {
                        ZuizuiPerf::SharedPerfData* pData = static_cast<ZuizuiPerf::SharedPerfData*>(
                            MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, sizeof(ZuizuiPerf::SharedPerfData))
                        );

                        if (pData != nullptr) {
                            if (pData->version == ZuizuiPerf::kSharedMemoryVersion) {
                                LiveMetricEntry entry;
                                entry.time = pData->time;
                                entry.fps = pData->fps;
                                entry.cpu_mb = pData->cpuMemoryMb;
                                entry.vram_mb = pData->vramMemoryMb;

                                static uint64_t s_lastFrameCount = 0;
                                if (pData->frameCount != s_lastFrameCount || s_liveMetrics.empty()) {
                                    s_lastFrameCount = pData->frameCount;
                                    s_liveMetrics.push_back(entry);
                                    if (s_liveMetrics.size() > kMaxLiveMetricsSize) {
                                        s_liveMetrics.erase(s_liveMetrics.begin());
                                    }
                                }
                            }
                            UnmapViewOfFile(pData);
                        }
                        CloseHandle(hMap);
                    }

                    std::string jsonResponse = "[\n";
                    for (size_t i = 0; i < s_liveMetrics.size(); ++i) {
                        jsonResponse += std::format(
                            "  {{\n"
                            "    \"time\": {:.3f},\n"
                            "    \"fps\": {:.2f},\n"
                            "    \"memory_mb\": {:.2f},\n"
                            "    \"vram_mb\": {:.2f}\n"
                            "  }}{}",
                            s_liveMetrics[i].time,
                            s_liveMetrics[i].fps,
                            s_liveMetrics[i].cpu_mb,
                            s_liveMetrics[i].vram_mb,
                            (i == s_liveMetrics.size() - 1 ? "" : ",\n")
                        );
                    }
                    jsonResponse += "\n]";

                    std::string response = std::format(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json; charset=utf-8\r\n"
                        "Content-Length: {}\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n{}",
                        jsonResponse.size(), jsonResponse
                    );
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    continue;
                }

                // Get Reports List API: GET /get_reports_list (Prefix match to ignore ?t=...)
                if (rawPath.find("/get_reports_list") == 0) {
                    std::string searchDirs[] = {
                        kTargetReportDir,
                        "../" + kTargetReportDir,
                        "../../" + kTargetReportDir,
                        "externals/PerformanceViewer/" + kTargetReportDir
                    };
                    std::string foundDir = "";
                    for (const auto& path : searchDirs) {
                        if (fs::exists(path)) {
                            foundDir = path;
                            break;
                        }
                    }

                    std::string jsonResponse = "[\n";
                    bool isFirst = true;

                    if (!foundDir.empty()) {
                        std::vector<fs::path> reportPaths;
                        
                        // Collect all report_ directories inside run_ directories
                        std::error_code ec;
                        for (auto runIt = fs::directory_iterator(foundDir, ec); runIt != fs::directory_iterator(); runIt.increment(ec)) {
                            if (ec) break;
                            if (runIt->is_directory(ec)) {
                                for (auto repIt = fs::directory_iterator(runIt->path(), ec); repIt != fs::directory_iterator(); repIt.increment(ec)) {
                                    if (ec) break;
                                    if (repIt->is_directory(ec) && repIt->path().filename().string().find("report_") == 0) {
                                        reportPaths.push_back(repIt->path());
                                    }
                                }
                            }
                        }


                        // Sort reports descending by directory name (latest date first)
                        std::sort(reportPaths.begin(), reportPaths.end(), [](const fs::path& a, const fs::path& b) {
                            return a.filename().string() > b.filename().string();
                        });

                        for (const auto& repPath : reportPaths) {
                            std::string logPath = (repPath / "system_log.json").string();
                            if (fs::exists(logPath)) {
                                std::ifstream logFile(logPath);
                                if (logFile.is_open()) {
                                    std::stringstream buffer;
                                    buffer << logFile.rdbuf();
                                    std::string content = buffer.str();

                                    // Simple extract lambda for json strings
                                    auto ExtractValue = [](const std::string& json, const std::string& key) -> std::string {
                                        size_t keyPos = json.find("\"" + key + "\"");
                                        if (keyPos == std::string::npos) return "UNKNOWN";
                                        size_t colonPos = json.find(":", keyPos);
                                        if (colonPos == std::string::npos) return "UNKNOWN";
                                        size_t startQuote = json.find("\"", colonPos);
                                        if (startQuote == std::string::npos) return "UNKNOWN";
                                        size_t endQuote = json.find("\"", startQuote + 1);
                                        if (endQuote == std::string::npos) return "UNKNOWN";
                                        return json.substr(startQuote + 1, endQuote - startQuote - 1);
                                    };

                                    std::string reason = ExtractValue(content, "reason");
                                    std::string detail = ExtractValue(content, "detail");
                                    std::string timeVal = ExtractValue(content, "time_triggered");

                                    std::string cleanPath = repPath.string();
                                    size_t slashPos = 0;
                                    while ((slashPos = cleanPath.find("\\", slashPos)) != std::string::npos) {
                                        cleanPath.replace(slashPos, 1, "/");
                                        slashPos += 1;
                                    }

                                    if (!isFirst) {
                                        jsonResponse += ",\n";
                                    }
                                    isFirst = false;

                                    jsonResponse += std::format(
                                        "  {{\n"
                                        "    \"path\": \"{}\",\n"
                                        "    \"time\": \"{}\",\n"
                                        "    \"reason\": \"{}\",\n"
                                        "    \"detail\": \"{}\"\n"
                                        "  }}",
                                        cleanPath, timeVal, reason, detail
                                    );
                                }
                            }
                        }
                    }

                    jsonResponse += "\n]";

                    std::string response = std::format(
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: application/json; charset=utf-8\r\n"
                        "Content-Length: {}\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Connection: close\r\n\r\n{}",
                        jsonResponse.size(), jsonResponse
                    );
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    continue;
                }

                std::string targetFilePath;
                std::string ext = fs::path(rawPath).extension().string();

                // Get clean path without query parameters for static asset resolution
                std::string cleanAssetPath = rawPath;
                size_t qPos = cleanAssetPath.find("?");
                if (qPos != std::string::npos) {
                    cleanAssetPath = cleanAssetPath.substr(0, qPos);
                }

                // Dispatch path (with multi-path fallback check)
                if (cleanAssetPath == "/index.html" || cleanAssetPath == "/style.css" || cleanAssetPath == "/app.js") {
                    // Try different working directories
                    std::string path1 = "." + cleanAssetPath;
                    std::string path2 = "externals/PerformanceViewer" + cleanAssetPath;
                    
                    if (fs::exists(path1)) {
                        targetFilePath = path1;
                    } else if (fs::exists(path2)) {
                        targetFilePath = path2;
                    }
                } else if (cleanAssetPath.find("/data/") == 0 && !s_reportPath.empty()) {
                    // Dump data (with relative path fallback check)
                    std::string subPath = cleanAssetPath.substr(5);
                    std::string path1 = s_reportPath + "/" + subPath;
                    std::string path2 = "../../" + s_reportPath + "/" + subPath;
                    std::string path3 = "../" + s_reportPath + "/" + subPath;

                    if (fs::exists(path1)) {
                        targetFilePath = path1;
                    } else if (fs::exists(path2)) {
                        targetFilePath = path2;
                    } else if (fs::exists(path3)) {
                        targetFilePath = path3;
                    }
                }

                if (!targetFilePath.empty() && fs::exists(targetFilePath)) {
                    SendFile(clientSocket, targetFilePath, GetMimeType(ext));
                } else {
                    std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                }
            }
        }
        closesocket(clientSocket);
    }

    // Shutdown
    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
