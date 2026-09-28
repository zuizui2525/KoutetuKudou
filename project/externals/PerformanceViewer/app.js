document.addEventListener("DOMContentLoaded", () => {
    // タブが閉じられる直前にサーバーの終了APIを呼び出す
    window.addEventListener("beforeunload", () => {
        navigator.sendBeacon("/exit");
    });

    // UI Elements
    const triggerReason = document.getElementById("trigger-reason");
    const triggerTime = document.getElementById("trigger-time");
    const exitBtn = document.getElementById("exit-btn");
    
    const replayVideo = document.getElementById("replay-video");
    const noReplayMsg = document.getElementById("no-replay-msg");
    const frameCounter = document.getElementById("frame-counter");

    const avgFpsEl = document.getElementById("avg-fps");
    const maxMemEl = document.getElementById("max-mem");
    const reportDetail = document.getElementById("report-detail");

    const copyBtn = document.getElementById("copy-btn");
    const promptText = document.getElementById("prompt-text");
    const spikeBanner = document.getElementById("spike-banner"); // アナウンスバナー
    const sidebarList = document.getElementById("sidebar-list"); // 履歴リスト用

    // Tab Elements
    const tabReplay = document.getElementById("tab-replay");
    const tabLive = document.getElementById("tab-live");
    const replayView = document.getElementById("replay-view");
    const liveView = document.getElementById("live-view");

    // Live Value Elements
    const liveValFps = document.getElementById("live-val-fps");
    const liveValCpu = document.getElementById("live-val-cpu");
    const liveValVram = document.getElementById("live-val-vram");

    // Chart Instances
    let metricsChart = null;
    let liveMetricsChart = null;
    
    let currentPath = null;
    let isLiveMode = false;

    // Initialize Mode Tabs
    setupTabs();

    // Start polling loop
    startPolling();

    // Setup tab click event handlers
    function setupTabs() {
        tabReplay.addEventListener("click", () => {
            isLiveMode = false;
            tabReplay.classList.add("active");
            tabLive.classList.remove("active");
            replayView.classList.add("active");
            liveView.classList.remove("active");
            
            // リプレイデータ表示
            if (currentPath) {
                init();
            } else {
                initEmptyState();
            }
        });

        tabLive.addEventListener("click", () => {
            isLiveMode = true;
            tabLive.classList.add("active");
            tabReplay.classList.remove("active");
            liveView.classList.add("active");
            replayView.classList.remove("active");
            
            // ライブデータを即時更新
            updateLiveMetrics();
        });
    }

    // Polling function (Runs at 300ms intervals for live response)
    function startPolling() {
        // 初回ロード
        loadReportsList();

        let counter = 0;
        setInterval(async () => {
            counter++;

            // 1) リアルタイム監視モード時の高速フェッチ (300ms間隔)
            if (isLiveMode) {
                updateLiveMetrics();
            }

            // 2) 定期パス監視・履歴リスト更新 (1200ms間隔に間引く)
            if (counter % 4 === 0) {
                try {
                    const res = await fetch("/get_latest_path?t=" + Date.now());
                    if (res.ok) {
                        const data = await res.json();
                        const latestPath = data.latest_path;
                        
                        // ビルドモード表示の更新
                        const buildModeEl = document.getElementById("build-mode");
                        if (buildModeEl && data.build_mode) {
                            buildModeEl.textContent = data.build_mode;
                            buildModeEl.className = "build-mode-tag build-mode-" + data.build_mode.toLowerCase();
                        }
                        
                        if (latestPath && latestPath !== "") {
                            if (currentPath === null) {
                                currentPath = latestPath;
                                init();
                            } else if (currentPath !== latestPath) {
                                // 新しいスパイクを検知！
                                currentPath = latestPath;
                                showSpikeAnnouncement();
                                // もしリプレイモードなら画面を自動リロード
                                if (!isLiveMode) {
                                    init();
                                }
                            }
                        } else if (currentPath === null) {
                            // レポートデータが1つもない起動時の初期化
                            initEmptyState();
                        }
                    }
                    loadReportsList();
                } catch (err) {
                    console.warn("Polling background check error:", err);
                }
            }
        }, 300);
    }

    // Fetch and draw live metrics
    async function updateLiveMetrics() {
        try {
            const res = await fetch("/get_live_metrics?t=" + Date.now());
            if (!res.ok) return;
            const logs = await res.json();

            if (!logs || logs.length === 0) {
                liveValFps.textContent = "待機中...";
                liveValCpu.textContent = "待機中...";
                liveValVram.textContent = "待機中...";
                return;
            }

            // 最新の値をカードに反映
            const latest = logs[logs.length - 1];
            liveValFps.textContent = latest.fps.toFixed(1);
            liveValCpu.textContent = latest.memory_mb.toFixed(1) + " MB";
            liveValVram.textContent = latest.vram_mb.toFixed(1) + " MB";

            // 統計データの更新
            let sumFps = 0;
            let minFps = 9999.0;
            let maxCpu = 0.0;
            let maxVram = 0.0;

            logs.forEach((log) => {
                sumFps += log.fps;
                if (log.fps < minFps) minFps = log.fps;
                if (log.memory_mb > maxCpu) maxCpu = log.memory_mb;
                if (log.vram_mb > maxVram) maxVram = log.vram_mb;
            });

            const avgFps = sumFps / logs.length;

            const liveStatAvgFps = document.getElementById("live-stat-avg-fps");
            const liveStatMinFps = document.getElementById("live-stat-min-fps");
            const liveStatMaxCpu = document.getElementById("live-stat-max-cpu");
            const liveStatMaxVram = document.getElementById("live-stat-max-vram");

            if (liveStatAvgFps) liveStatAvgFps.textContent = avgFps.toFixed(1);
            if (liveStatMinFps) liveStatMinFps.textContent = minFps.toFixed(1);
            if (liveStatMaxCpu) liveStatMaxCpu.textContent = maxCpu.toFixed(1) + " MB";
            if (liveStatMaxVram) liveStatMaxVram.textContent = maxVram.toFixed(1) + " MB";

            // リアルタイムグラフの更新
            setupLiveMetricsChart(logs);

        } catch (err) {
            console.error("Failed to update live metrics:", err);
        }
    }

    // Render live rolling chart (No animation for peak performance)
    function setupLiveMetricsChart(logs) {
        const labels = [];
        const fpsData = [];
        const memData = [];
        const vramData = [];

        logs.forEach((log) => {
            labels.push(log.time.toFixed(1) + "秒");
            fpsData.push(log.fps);
            memData.push(log.memory_mb);
            vramData.push(log.vram_mb);
        });

        const ctx = document.getElementById('liveMetricsChart').getContext('2d');

        if (liveMetricsChart) {
            liveMetricsChart.data.labels = labels;
            liveMetricsChart.data.datasets[0].data = fpsData;
            liveMetricsChart.data.datasets[1].data = memData;
            liveMetricsChart.data.datasets[2].data = vramData;
            liveMetricsChart.update();
            return;
        }

        // 新規作成時のみアニメーションなしで初期化
        liveMetricsChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: '現在の FPS',
                        data: fpsData,
                        borderColor: '#00f2fe',
                        backgroundColor: 'rgba(0, 242, 254, 0.05)',
                        borderWidth: 2,
                        pointRadius: 0,
                        pointHoverRadius: 4,
                        yAxisID: 'y-fps',
                        tension: 0.3
                    },
                    {
                        label: 'CPU メモリ (MB)',
                        data: memData,
                        borderColor: '#a78bfa',
                        backgroundColor: 'rgba(167, 139, 250, 0.01)',
                        borderWidth: 1.5,
                        pointRadius: 0,
                        pointHoverRadius: 3,
                        borderDash: [4, 4],
                        yAxisID: 'y-mem',
                        tension: 0.1
                    },
                    {
                        label: 'GPU VRAM (MB)',
                        data: vramData,
                        borderColor: '#f59e0b',
                        backgroundColor: 'rgba(245, 158, 11, 0.01)',
                        borderWidth: 1.5,
                        pointRadius: 0,
                        pointHoverRadius: 3,
                        yAxisID: 'y-mem',
                        tension: 0.1
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                plugins: {
                    legend: {
                        labels: { color: '#e2e8f0', font: { family: 'Outfit', size: 11 } }
                    }
                },
                scales: {
                    x: {
                        grid: { color: 'rgba(255,255,255,0.03)' },
                        ticks: { color: '#94a3b8', font: { family: 'Outfit', size: 10 } }
                    },
                    'y-fps': {
                        type: 'linear',
                        position: 'left',
                        grid: { color: 'rgba(255,255,255,0.03)' },
                        ticks: { color: '#00f2fe', font: { family: 'Outfit', size: 10 } },
                        min: 0,
                        max: 90
                    },
                    'y-mem': {
                        type: 'linear',
                        position: 'right',
                        grid: { display: false },
                        ticks: { color: '#a78bfa', font: { family: 'Outfit', size: 10 } }
                    }
                }
            }
        });
    }

    // Load past reports list for the sidebar
    async function loadReportsList() {
        if (!sidebarList) return;
        try {
            const res = await fetch("/get_reports_list?t=" + Date.now());
            if (!res.ok) throw new Error("Failed to load reports list");
            const reports = await res.json();

            if (!reports || reports.length === 0) {
                sidebarList.innerHTML = `<li class="loading-item">履歴はありません。</li>`;
                return;
            }

            let listHtml = "";
            reports.forEach((rep) => {
                const isActive = (currentPath && currentPath.replace(/\\/g, "/") === rep.path.replace(/\\/g, "/"));
                const activeClass = isActive ? "active" : "";
                
                let reasonText = rep.reason;
                let reasonClass = "fps-drop";
                if (rep.reason === "FPS_DROP") {
                    reasonText = "FPS低下";
                    reasonClass = "fps-drop";
                } else if (rep.reason === "LONG_LOAD") {
                    reasonText = "低速ロード";
                    reasonClass = "long-load";
                }

                listHtml += `
                    <li class="sidebar-item ${activeClass}" data-path="${rep.path}">
                        <div class="sidebar-item-header">
                            <span class="sidebar-item-time">${rep.time}</span>
                            <span class="sidebar-item-reason ${reasonClass}">${reasonText}</span>
                        </div>
                        <div class="sidebar-item-detail">${rep.detail}</div>
                    </li>
                `;
            });

            sidebarList.innerHTML = listHtml;

            // 起動時にレポートが1つもあり、現在選択中のレポートがない場合は自動で最新のレポートを選択する
            if (currentPath === null && reports.length > 0) {
                currentPath = reports[0].path;
                await fetch(`/update_path?path=${encodeURIComponent(currentPath)}&t=` + Date.now());
                if (!isLiveMode) {
                    init();
                }
            }

            const items = sidebarList.querySelectorAll(".sidebar-item");
            items.forEach((item) => {
                item.addEventListener("click", async () => {
                    const path = item.getAttribute("data-path");
                    if (path && path !== currentPath) {
                        currentPath = path;
                        await fetch(`/update_path?path=${encodeURIComponent(path)}&t=` + Date.now());
                        
                        // 自動的に「リプレイ解析モード」タブへ戻して詳細を表示する
                        isLiveMode = false;
                        tabReplay.classList.add("active");
                        tabLive.classList.remove("active");
                        replayView.classList.add("active");
                        liveView.classList.remove("active");
                        
                        init();
                    }
                });
            });

        } catch (err) {
            console.error("Failed to load sidebar list:", err);
        }
    }

    // Show visual announcement when new spike occurs
    function showSpikeAnnouncement() {
        if (spikeBanner) {
            spikeBanner.classList.add("visible");
            setTimeout(() => {
                spikeBanner.classList.remove("visible");
            }, 6000);
        }
    }

    // Initialize Empty State (No spike data yet)
    function initEmptyState() {
        triggerReason.className = "badge live-monitoring";
        triggerReason.textContent = "監視中";
        triggerTime.textContent = "スパイク未検出";
        reportDetail.textContent = "現在ゲームのパフォーマンスをリアルタイム監視しています。";

        noReplayMsg.textContent = "リプレイデータはまだありません。ゲーム中に重くなるとここに自動で動画が表示されます。";
        noReplayMsg.classList.remove("hidden");
        if (replayVideo) replayVideo.classList.add("hidden");

        avgFpsEl.textContent = "平均 FPS: --";
        maxMemEl.textContent = "最大メモリ: -- MB";

        if (metricsChart) {
            metricsChart.destroy();
            metricsChart = null;
        }

        promptText.value = "スパイクが検出されると、ここにAI解析用のプロンプトが自動生成されます。";
    }

    async function init() {
        if (!currentPath) {
            initEmptyState();
            return;
        }

        try {
            // Load JSON Data (Cache-Busted)
            const res = await fetch("/data/system_log.json?t=" + Date.now());
            if (!res.ok) throw new Error("Log JSON not found");
            const data = await res.json();

            // Populate Metadata
            let reasonJa = data.reason || "不明";
            triggerReason.className = "badge"; // Reset
            if (data.reason === "LONG_LOAD") {
                reasonJa = "低速ロード";
                triggerReason.classList.add("long-load");
            } else {
                reasonJa = "FPS低下検出";
                triggerReason.classList.add("fps-drop");
            }
            triggerReason.textContent = reasonJa;
            triggerTime.textContent = data.time_triggered || "不明な時刻";
            reportDetail.textContent = data.detail || "詳細情報はありません。";

            // Generate Advanced AI Prompt based on system_log.json data (Fallback: Load prompt.md if generation fails)
            try {
                promptText.value = generateAIPrompt(data);
            } catch (promptErr) {
                console.warn("Failed to generate AI prompt dynamically:", promptErr);
                const promptRes = await fetch("/data/prompt.md?t=" + Date.now());
                if (promptRes.ok) {
                    promptText.value = await promptRes.text();
                }
            }

            // Setup Performance Metrics & Chart
            setupMetrics(data.logs);

            // Load MP4 Replay
            setupReplayVideo();

            // Hide error message if success
            noReplayMsg.classList.add("hidden");
            if (replayVideo) replayVideo.classList.remove("hidden");

            // 最新の重要システムログを抽出して右側に表示
            if (data.system_logs) {
                updateLiveLogPanel(data.system_logs);
            }

        } catch (err) {
            console.error("Initialization error:", err);
            initEmptyState();
        }
    }

    // 警告・エラー・ロード遅延のシステムログをフィルタリングして表示する処理
    function updateLiveLogPanel(systemLogs) {
        const liveLogContainer = document.getElementById("live-log-container");
        if (!liveLogContainer) return;

        if (!systemLogs || systemLogs.length === 0) {
            liveLogContainer.innerHTML = '<div class="log-empty-msg">警告や低速処理は検出されていません。</div>';
            return;
        }

        const filteredLogs = systemLogs.filter(log => {
            const lower = log.toLowerCase();
            return lower.includes("[警告]") || lower.includes("warn") || lower.includes("error") || lower.includes("fail") || lower.includes("低速") || lower.includes("load");
        });

        if (filteredLogs.length === 0) {
            liveLogContainer.innerHTML = '<div class="log-empty-msg">警告や低速処理は検出されていません。</div>';
            return;
        }

        let logHtml = "";
        filteredLogs.forEach(log => {
            let className = "log-entry-warn";
            if (log.toLowerCase().includes("error") || log.toLowerCase().includes("fail")) {
                className = "log-entry-error";
            }
            logHtml += `<div class="${className}">${escapeHtml(log)}</div>`;
        });

        liveLogContainer.innerHTML = logHtml;
        liveLogContainer.scrollTop = liveLogContainer.scrollHeight;
    }

    function escapeHtml(str) {
        return str.replace(/[&<>'"]/g, 
            tag => ({
                '&': '&amp;',
                '<': '&lt;',
                '>': '&gt;',
                "'": '&#39;',
                '"': '&quot;'
            }[tag] || tag)
        );
    }

    // Performance Data Calculation & Chart Rendering (Cache-Busted)
    function setupMetrics(logs) {
        if (!logs || logs.length === 0) return;

        let totalFps = 0;
        let peakMem = 0;
        const labels = [];
        const fpsData = [];
        const memData = [];
        const vramData = [];

        logs.forEach((log) => {
            totalFps += log.fps;
            if (log.memory_mb > peakMem) {
                peakMem = log.memory_mb;
            }
            labels.push(log.time.toFixed(1) + "秒");
            fpsData.push(log.fps);
            memData.push(log.memory_mb);
            vramData.push(log.vram_mb || 0);
        });

        const avgFps = totalFps / logs.length;
        avgFpsEl.textContent = `平均 FPS: ${avgFps.toFixed(1)}`;
        maxMemEl.textContent = `最大メモリ: ${peakMem.toFixed(1)} MB`;

        const ctx = document.getElementById('metricsChart').getContext('2d');
        
        if (metricsChart) {
            metricsChart.destroy();
        }

        metricsChart = new Chart(ctx, {
            type: 'line',
            data: {
                labels: labels,
                datasets: [
                    {
                        label: 'FPS',
                        data: fpsData,
                        borderColor: '#00f2fe',
                        backgroundColor: 'rgba(0, 242, 254, 0.05)',
                        borderWidth: 2,
                        pointBackgroundColor: '#00f2fe',
                        pointRadius: 0,
                        pointHoverRadius: 4,
                        yAxisID: 'y-fps',
                        tension: 0.3
                    },
                    {
                        label: 'CPU メモリ (MB)',
                        data: memData,
                        borderColor: '#a78bfa',
                        backgroundColor: 'rgba(167, 139, 250, 0.02)',
                        borderWidth: 1.5,
                        pointRadius: 0,
                        pointHoverRadius: 3,
                        borderDash: [4, 4],
                        yAxisID: 'y-mem',
                        tension: 0.1
                    },
                    {
                        label: 'GPU VRAM (MB)',
                        data: vramData,
                        borderColor: '#f59e0b',
                        backgroundColor: 'rgba(245, 158, 11, 0.02)',
                        borderWidth: 1.5,
                        pointRadius: 0,
                        pointHoverRadius: 3,
                        yAxisID: 'y-mem',
                        tension: 0.1
                    }
                ]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                animation: false,
                plugins: {
                    legend: {
                        labels: { color: '#e2e8f0', font: { family: 'Outfit', size: 11 } }
                    }
                },
                scales: {
                    x: {
                        grid: { color: 'rgba(255,255,255,0.03)' },
                        ticks: { color: '#94a3b8', font: { family: 'Outfit', size: 10 } }
                    },
                    'y-fps': {
                        type: 'linear',
                        position: 'left',
                        grid: { color: 'rgba(255,255,255,0.03)' },
                        ticks: { color: '#00f2fe', font: { family: 'Outfit', size: 10 } },
                        min: 0,
                        max: 90
                    },
                    'y-mem': {
                        type: 'linear',
                        position: 'right',
                        grid: { display: false },
                        ticks: { color: '#a78bfa', font: { family: 'Outfit', size: 10 } }
                    }
                }
            }
        });
    }

    // Video Setup (Cache-Busted)
    async function setupReplayVideo() {
        const url = "/data/replay.mp4?t=" + Date.now(); // Cache-Busted URL
        try {
            if (replayVideo) {
                replayVideo.src = url;
                replayVideo.load();
                noReplayMsg.classList.add("hidden");
                replayVideo.classList.remove("hidden");
            }
            if (frameCounter) {
                frameCounter.textContent = "録画データを読み込みました";
            }
        } catch (err) {
            console.error("Video load error:", err);
            noReplayMsg.textContent = "録画データ（MP4）が見つかりません";
            noReplayMsg.classList.remove("hidden");
            if (replayVideo) replayVideo.classList.add("hidden");
        }
    }

    // Copy Prompt Button Control
    copyBtn.addEventListener("click", async () => {
        try {
            await navigator.clipboard.writeText(promptText.value);
            copyBtn.textContent = "コピー完了！";
            copyBtn.style.background = "linear-gradient(135deg, #10b981 0%, #059669 100%)";
            copyBtn.style.boxShadow = "0 4px 15px rgba(16, 185, 129, 0.4)";
            
            setTimeout(() => {
                copyBtn.textContent = "プロンプトをコピー";
                copyBtn.style.background = "";
                copyBtn.style.boxShadow = "";
            }, 2000);
        } catch (err) {
            alert("クリップボードへのコピーに失敗しました。");
        }
    });

    // Close and Shutdown Server
    exitBtn.addEventListener("click", async () => {
        if (confirm("サーバーを終了し、ビューワーを閉じますか？")) {
            try {
                await fetch("/exit");
            } catch (err) {
                // Expected disconnect error
            }
            window.close();
            document.body.innerHTML = `
                <div style="display:flex; justify-content:center; align-items:center; height:100vh; background:#0a0e1a; color:#ef4444; font-family:Outfit, sans-serif;">
                    <div style="text-align:center;">
                        <h1 style="font-size:32px; margin-bottom:16px;">サーバーは正常に終了しました</h1>
                        <p style="color:#64748b;">このウィンドウは安全に閉じることができます。</p>
                    </div>
                </div>
            `;
        }
    });

    // Generate advanced AI prompt dynamically based on system_log.json contents
    function generateAIPrompt(jsonObj) {
        const reason = jsonObj.reason || "不明";
        const detail = jsonObj.detail || "詳細情報はありません。";
        const time = jsonObj.time_triggered || "不明な時刻";
        const res = jsonObj.resolution || "不明";
        const logs = jsonObj.logs || [];
        const sysLogs = jsonObj.system_logs || [];

        // Performance log metrics calculation
        let minFps = 999;
        let minFpsTime = 0;
        let maxMem = 0;
        let maxVram = 0;
        const startMem = logs.length > 0 ? logs[0].memory_mb : 0;
        const endMem = logs.length > 0 ? logs[logs.length - 1].memory_mb : 0;
        const startVram = logs.length > 0 ? logs[0].vram_mb : 0;
        const endVram = logs.length > 0 ? logs[logs.length - 1].vram_mb : 0;

        logs.forEach(log => {
            if (log.fps < minFps) {
                minFps = log.fps;
                minFpsTime = log.time;
            }
            if (log.memory_mb > maxMem) maxMem = log.memory_mb;
            if (log.vram_mb > maxVram) maxVram = log.vram_mb;
        });

        const reasonJa = reason === "LONG_LOAD" ? "低速ロード" : "FPS低下検出";

        let prompt = `あなたはC++およびDirectX12ゲームエンジンのパフォーマンス最適化の超一流エキスパートエンジニアです。
以下の実行時パフォーマンスデータおよびゲームエンジン内のログメッセージをもとに、FPS低下が発生した原因を推測し、考えられるボトルネックの特定と具体的なコードレベルでの修正案（最適化案）を日本語で提示してください。

# 【AI解析依頼】パフォーマンス低下スパイクの調査

## 1. 発生時の詳細コンテキスト
- **検知トリガー理由**: \`${reasonJa}\`
- **詳細情報**: ${detail}
- **発生日時**: ${time}
- **画面解像度**: ${res}

## 2. パフォーマンス集計データ
- **最低 FPS**: **${minFps.toFixed(1)} FPS** (経過時間 ${minFpsTime.toFixed(2)} 秒時点で検出)
- **最大 CPUメモリ使用量**: **${maxMem.toFixed(1)} MB** (開始時: ${startMem.toFixed(1)} MB -> 終了時: ${endMem.toFixed(1)} MB)
- **最大 GPU VRAM使用量**: **${maxVram.toFixed(1)} MB** (開始時: ${startVram.toFixed(1)} MB -> 終了時: ${endVram.toFixed(1)} MB)

## 3. 時系列パフォーマンスログ (抜粋)
| 経過時間 | FPS | CPUメモリ | GPU VRAM |
| :--- | :--- | :--- | :--- |
`;

        const step = Math.max(1, Math.floor(logs.length / 25));
        for (let i = 0; i < logs.length; i += step) {
            const log = logs[i];
            prompt += `| ${log.time.toFixed(2)}s | ${log.fps.toFixed(1)} | ${log.memory_mb.toFixed(1)} MB | ${log.vram_mb.toFixed(1)} MB |\n`;
        }

        // Heavy load tasks and warning/error extraction from engine logs
        const heavyLoads = [];
        const warnings = [];
        sysLogs.forEach(msg => {
            if (msg.includes("低速ロード") || msg.includes("ロード完了") || msg.includes("コンパイル完了")) {
                heavyLoads.push(msg);
            }
            if (msg.includes("[警告]") || msg.toLowerCase().includes("error") || msg.toLowerCase().includes("failed") || msg.includes("エラー")) {
                if (!msg.includes("パフォーマンスレポートとMP4動画")) {
                    warnings.push(msg);
                }
            }
        });

        prompt += "\n## 4. 直近のログから検出された具体的な処理・警告 (ボトルネックのヒント)\n";
        if (heavyLoads.length > 0) {
            prompt += "### 高負荷なロード・コンパイル処理 (0.1秒以上)\n";
            heavyLoads.forEach(l => {
                prompt += `- ${l}\n`;
            });
        } else {
            prompt += "### 高負荷なロード・コンパイル処理\n- ログ上に0.1秒以上かかったロードやコンパイル処理は検出されませんでした。\n";
        }

        if (warnings.length > 0) {
            prompt += "\n### 警告・エラーログ\n";
            warnings.forEach(w => {
                prompt += `- ${w}\n`;
            });
        } else {
            prompt += "\n### 警告・エラーログ\n- ログ上に警告やエラーは検出されませんでした。\n";
        }

        prompt += `\n## 5. あなた（LLM）への具体的な調査指示事項
1. **最低FPS発生時の分析**: 経過時間 **${minFpsTime.toFixed(2)}秒** 付近で最低の **${minFps.toFixed(1)} FPS** を記録しています。この瞬間のメモリ・VRAMおよび周辺のログメッセージから、何がボトルネックであったかを特定してください。
2. **メモリ推移の検証**: 開始時から終了時にかけて、CPUメモリが ${(endMem - startMem).toFixed(1)} MB、GPU VRAMが ${(endVram - startVram).toFixed(1)} MB 変化しています。急激な増加やメモリリークの兆候が見られるか診断してください。`;

        if (heavyLoads.length > 0) {
            prompt += `\n3. **検出されたアセットロード処理の非同期化・軽量化検討**: ログに記録されたロード処理について、同期ロード of 回避方法やアセット軽量化の提案を記述してください。`;
        } else {
            prompt += `\n3. **非ロード時のボトルネック分析**: ファイルI/O起因ではないFPS低下が疑われます。毎フレームの描画コール数や頂点バッファ、ロジック更新処理のいずれかでストールが発生している可能性が高いため、その観点から推測してください。`;
        }

        return prompt;
    }
});
