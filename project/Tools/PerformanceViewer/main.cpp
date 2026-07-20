#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <format>
#include <sstream>
#include <algorithm>

// Suppress console window and run as windows application with 'main' entry
#pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#pragma comment(lib, "ws2_32.lib")

namespace fs = std::filesystem;

namespace {
    constexpr int kPort = 8080;
    std::string s_reportPath = "";

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
        std::string target = "out/performance_reports";
        
        // Search current dir, parent dir, or parent parent dir
        std::string searchPaths[] = { target, "../" + target, "../../" + target };
        std::string foundTarget = "";
        for (const auto& path : searchPaths) {
            if (fs::exists(path)) {
                foundTarget = path;
                break;
            }
        }

        if (foundTarget.empty()) {
            return "";
        }

        fs::path latestPath;
        std::time_t latestTime = 0;

        for (const auto& entry : fs::directory_iterator(foundTarget)) {
            if (entry.is_directory()) {
                for (const auto& subEntry : fs::directory_iterator(entry.path())) {
                    if (subEntry.is_directory() && subEntry.path().filename().string().find("report_") == 0) {
                        auto writeTime = fs::last_write_time(subEntry);
                        auto sct = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                            writeTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
                        std::time_t t = std::chrono::system_clock::to_time_t(sct);

                        if (t > latestTime) {
                            latestTime = t;
                            latestPath = subEntry.path();
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
    // Determine report folder
    if (argc > 1) {
        s_reportPath = argv[1];
    } else {
        s_reportPath = FindLatestReport();
    }

    // Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(kPort);

    // Bind port
    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Start listening
    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    // Launch browser automatically
    std::string url = std::format("http://localhost:{}/", kPort);
    ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);

    // HTTP Request Loop
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

            if (method == "GET") {
                // Normalize path
                if (rawPath == "/") {
                    rawPath = "/index.html";
                }

                // Shutdown API (Prefix match to support optional queries)
                if (rawPath.find("/exit") == 0) {
                    std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                    send(clientSocket, response.c_str(), (int)response.size(), 0);
                    closesocket(clientSocket);
                    running = false;
                    break;
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
                    
                    std::string jsonResponse = std::format("{{\"latest_path\": \"{}\"}}", cleanPath);
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
                    std::string jsonResponse = "[\n";
                    for (size_t i = 0; i < s_liveMetrics.size(); ++i) {
                        jsonResponse += std::format(
                            "  {{\n"
                            "    \"time\": {},\n"
                            "    \"fps\": {},\n"
                            "    \"memory_mb\": {},\n"
                            "    \"vram_mb\": {}\n"
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
                    std::string targetDir = "out/performance_reports";
                    std::string searchDirs[] = { targetDir, "../" + targetDir, "../../" + targetDir };
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
                        for (const auto& runEntry : fs::directory_iterator(foundDir)) {
                            if (runEntry.is_directory()) {
                                for (const auto& repEntry : fs::directory_iterator(runEntry.path())) {
                                    if (repEntry.is_directory() && repEntry.path().filename().string().find("report_") == 0) {
                                        reportPaths.push_back(repEntry.path());
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
                    std::string path2 = "Tools/PerformanceViewer" + cleanAssetPath;
                    
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
