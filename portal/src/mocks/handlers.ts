import { http, HttpResponse, delay } from "msw";

export const handlers = [
  // Device information
  http.get("/api/v1/device", async () => {
    await delay(200);
    return HttpResponse.json({
      status: "success",
      data: {
        model: "ESP32-Gateway-Mock",
        firmware_version: "v1.0.0-dev",
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
        sta_active: false,
        sta_connecting: false,
        sta_connected: false,
        sta_error: "",
        ip: "",
        disconnect_reason: 0,
      },
    });
  }),

  // Wi-Fi credentials (POST)
  http.post("/api/v1/wifi/credentials", async ({ request }) => {
    await delay(300);
    const body = await request.json();
    console.log("Mock: Received Wi-Fi credentials", body);

    return HttpResponse.json({
      status: "success",
      data: {
        restart_required: false,
        sta_connect_started: true,
        sta_error: "",
      },
    });
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
