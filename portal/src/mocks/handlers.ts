import { http, HttpResponse, delay } from "msw";

const wifiState = {
  sta_connected: false,
  sta_error: "",
  ip: "",
  disconnect_reason: 0,
  connectedSsid: "",
};

export const handlers = [
  // Health check
  http.get("/health", async () => {
    await delay(50);

    return HttpResponse.json({
      status: "success",
      data: {
        status: "ok",
        uptime: Math.floor(Date.now() / 1000) % 86400, // Seconds in current day
        version: "v1.0.0-dev",
      },
    });
  }),

  // Portal detail
  http.get("/api/v1/portal", async () => {
    await delay(100);
    return HttpResponse.json({
      status: "success",
      data: {
        title: "ESP Gateway Portal",
      },
    });
  }),

  // Device information
  http.get("/api/v1/device", async () => {
    await delay(200);
    return HttpResponse.json({
      status: "success",
      data: {
        model: "ESP32-Gateway-Mock",
        gateway_version: "v1.0.0-dev",
        build_time: "2025-10-09 12:00:00",
        idf_version: "v5.0.0",
      },
    });
  }),

  // System metrics
  http.get("/api/v1/metrics", async () => {
    await delay(150);
    const total = 327680;
    const used = Math.floor(total * 0.35);
    return HttpResponse.json({
      status: "success",
      data: {
        heap_total: total,
        heap_free: total - used,
        heap_used: used,
        heap_min_free: 98304,
        heap_largest_free_block: 65536,
        timestamp_ms: Date.now(),
      },
    });
  }),

  // mDNS information
  http.get("/api/v1/mdns", async () => {
    await delay(100);
    return HttpResponse.json({
      status: "success",
      data: {
        hostname: "esp-gateway-mock",
        instance_name: "ESP Gateway Mock",
        service_type: "_http",
        protocol: "_tcp",
        port: 80,
        running: true,
      },
    });
  }),

  // Wi-Fi status
  http.get("/api/v1/wifi/status", async () => {
    await delay(100);
    return HttpResponse.json({
      status: "success",
      data: {
        ap_active: true,
        sta_active: wifiState.sta_connected,
        sta_connected: wifiState.sta_connected,
        sta_error: wifiState.sta_error,
        ip: wifiState.ip,
        disconnect_reason: wifiState.disconnect_reason,
      },
    });
  }),

  // Wi-Fi scan
  http.get("/api/v1/wifi/scan", async () => {
    await delay(800); // Simulate scan time
    return HttpResponse.json({
      status: "success",
      data: {
        networks: [
          {
            ssid: "MyHomeNetwork",
            bssid: "aa:bb:cc:dd:ee:01",
            signal: 85,
            security: "WPA2",
            connected: wifiState.connectedSsid === "MyHomeNetwork" && wifiState.sta_connected,
            hidden: false,
            rssi: -45,
            channel: 6,
          },
          {
            ssid: "OfficeWiFi",
            bssid: "aa:bb:cc:dd:ee:02",
            signal: 72,
            security: "WPA2/WPA3",
            connected: wifiState.connectedSsid === "OfficeWiFi" && wifiState.sta_connected,
            hidden: false,
            rssi: -58,
            channel: 11,
          },
          {
            ssid: "GuestNetwork",
            bssid: "aa:bb:cc:dd:ee:03",
            signal: 60,
            security: "Open",
            connected: wifiState.connectedSsid === "GuestNetwork" && wifiState.sta_connected,
            hidden: false,
            rssi: -65,
            channel: 1,
          },
          {
            ssid: "Neighbor_2.4GHz",
            bssid: "aa:bb:cc:dd:ee:04",
            signal: 45,
            security: "WPA2",
            connected: wifiState.connectedSsid === "Neighbor_2.4GHz" && wifiState.sta_connected,
            hidden: false,
            rssi: -75,
            channel: 3,
          },
          {
            ssid: "CoffeeShop",
            bssid: "aa:bb:cc:dd:ee:05",
            signal: 38,
            security: "Open",
            connected: wifiState.connectedSsid === "CoffeeShop" && wifiState.sta_connected,
            hidden: false,
            rssi: -80,
            channel: 9,
          },
          {
            ssid: "SecureNetwork",
            bssid: "aa:bb:cc:dd:ee:06",
            signal: 55,
            security: "WPA3",
            connected: wifiState.connectedSsid === "SecureNetwork" && wifiState.sta_connected,
            hidden: false,
            rssi: -70,
            channel: 6,
          },
        ],
        error: "",
      },
    });
  }),

  // Wi-Fi credentials (POST) - Save only
  http.post("/api/v1/wifi/credentials", async ({ request }) => {
    await delay(300);
    const body = (await request.json()) as { ssid: string; passphrase: string };
    console.log("Mock: Saved Wi-Fi credentials", body);

    return HttpResponse.json({
      status: "success",
      data: {},
    });
  }),

  // Wi-Fi connect (POST) - Connect using saved credentials
  http.post("/api/v1/wifi/connect", async () => {
    // Check if there are saved credentials (simulated - always assume there are)
    const savedSsid = "MyHomeNetwork"; // Simulate saved SSID

    console.log("Mock: Attempting to connect using saved credentials");

    // Simulate connection attempt (takes ~2 seconds)
    await delay(2000);

    // Simulate successful connection (90% success rate)
    const isSuccess = Math.random() > 0.1;

    if (isSuccess) {
      wifiState.sta_connected = true;
      wifiState.sta_error = "";
      wifiState.ip = "192.168.1." + Math.floor(Math.random() * 200 + 10);
      wifiState.connectedSsid = savedSsid;
      wifiState.disconnect_reason = 0;
      console.log(`Mock: Successfully connected to ${savedSsid}`);

      return HttpResponse.json({
        status: "success",
        data: {},
      });
    } else {
      // Simulate connection failure
      wifiState.sta_connected = false;
      wifiState.sta_error = "Connection timeout";
      wifiState.ip = "";
      wifiState.connectedSsid = "";
      wifiState.disconnect_reason = 2; // AUTH_EXPIRE
      console.log(`Mock: Failed to connect to ${savedSsid}`);

      return HttpResponse.json({
        status: "error",
        error: {
          message: "Connection timeout",
          code: "ESP_ERR_TIMEOUT",
      },
      }, { status: 500 });
    }
  }),

  // Device logs
  http.get("/api/v1/logs", async () => {
    await delay(200);
    const now = Date.now();
    const entries = [
      {
        id: 1,
        timestamp_ms: now - 30000,
        level: "info" as const,
        tag: "boot",
        message: "ESP-IDF v5.0.0 boot sequence initiated",
      },
      {
        id: 2,
        timestamp_ms: now - 29000,
        level: "debug" as const,
        tag: "nvs",
        message: "NVS partition initialized successfully",
      },
      {
        id: 3,
        timestamp_ms: now - 28000,
        level: "info" as const,
        tag: "wifi",
        message: "Wi-Fi driver initialized",
      },
      {
        id: 4,
        timestamp_ms: now - 27000,
        level: "info" as const,
        tag: "wifi",
        message: "Starting Wi-Fi AP mode: ESP-Gateway-AP",
      },
      {
        id: 5,
        timestamp_ms: now - 26000,
        level: "info" as const,
        tag: "wifi",
        message: "AP started successfully, channel 1",
      },
      {
        id: 6,
        timestamp_ms: now - 25000,
        level: "debug" as const,
        tag: "tcpip",
        message: "TCP/IP adapter initialized",
      },
      {
        id: 7,
        timestamp_ms: now - 24000,
        level: "info" as const,
        tag: "mdns",
        message: "mDNS service initializing...",
      },
      {
        id: 8,
        timestamp_ms: now - 23000,
        level: "info" as const,
        tag: "mdns",
        message: "mDNS hostname set to: esp-gateway-mock.local",
      },
      {
        id: 9,
        timestamp_ms: now - 22000,
        level: "info" as const,
        tag: "mdns",
        message: "mDNS service started successfully",
      },
      {
        id: 10,
        timestamp_ms: now - 21000,
        level: "warn" as const,
        tag: "wifi",
        message: "STA mode not configured, skipping connection",
      },
      {
        id: 11,
        timestamp_ms: now - 20000,
        level: "info" as const,
        tag: "http",
        message: "HTTP server starting on port 80",
      },
      {
        id: 12,
        timestamp_ms: now - 19000,
        level: "debug" as const,
        tag: "http",
        message: "Registering URI handlers for REST API",
      },
      {
        id: 13,
        timestamp_ms: now - 18000,
        level: "info" as const,
        tag: "http",
        message: "HTTP server started successfully",
      },
      {
        id: 14,
        timestamp_ms: now - 17000,
        level: "debug" as const,
        tag: "system",
        message: "Free heap: 227KB / Total heap: 320KB",
      },
      {
        id: 15,
        timestamp_ms: now - 16000,
        level: "info" as const,
        tag: "system",
        message: "System initialization complete",
      },
      {
        id: 16,
        timestamp_ms: now - 15000,
        level: "debug" as const,
        tag: "wifi",
        message: "AP client count: 0",
      },
      {
        id: 17,
        timestamp_ms: now - 14000,
        level: "info" as const,
        tag: "http",
        message: "GET /api/v1/device - 200 OK",
      },
      {
        id: 18,
        timestamp_ms: now - 13000,
        level: "debug" as const,
        tag: "system",
        message: "Periodic metrics collection triggered",
      },
      {
        id: 19,
        timestamp_ms: now - 12000,
        level: "debug" as const,
        tag: "system",
        message: "Free heap: 225KB / Total heap: 320KB",
      },
      {
        id: 20,
        timestamp_ms: now - 11000,
        level: "info" as const,
        tag: "http",
        message: "GET /api/v1/metrics - 200 OK",
      },
      {
        id: 21,
        timestamp_ms: now - 10000,
        level: "info" as const,
        tag: "http",
        message: "GET /api/v1/mdns - 200 OK",
      },
      {
        id: 22,
        timestamp_ms: now - 9000,
        level: "debug" as const,
        tag: "wifi",
        message: "AP beacon transmission OK",
      },
      {
        id: 23,
        timestamp_ms: now - 8000,
        level: "info" as const,
        tag: "http",
        message: "GET /api/v1/logs - 200 OK",
      },
      {
        id: 24,
        timestamp_ms: now - 7000,
        level: "debug" as const,
        tag: "system",
        message: "Task watchdog triggered for idle task",
      },
      {
        id: 25,
        timestamp_ms: now - 6000,
        level: "debug" as const,
        tag: "system",
        message: "Free heap: 223KB / Total heap: 320KB",
      },
      {
        id: 26,
        timestamp_ms: now - 5000,
        level: "info" as const,
        tag: "http",
        message: "GET /api/v1/metrics - 200 OK",
      },
      {
        id: 27,
        timestamp_ms: now - 4000,
        level: "debug" as const,
        tag: "wifi",
        message: "AP client count: 1 (new connection)",
      },
      {
        id: 28,
        timestamp_ms: now - 3000,
        level: "info" as const,
        tag: "wifi",
        message: "Client connected to AP: MAC xx:xx:xx:xx:xx:01",
      },
      {
        id: 29,
        timestamp_ms: now - 2000,
        level: "debug" as const,
        tag: "dhcp",
        message: "DHCP lease assigned: 192.168.4.2",
      },
      {
        id: 30,
        timestamp_ms: now - 1000,
        level: "info" as const,
        tag: "http",
        message: "GET /api/v1/logs - 200 OK",
      },
    ];

    return HttpResponse.json({
      status: "success",
      data: {
        entries,
        next_cursor: 31,
        has_more: true,
      },
    });
  }),
];
