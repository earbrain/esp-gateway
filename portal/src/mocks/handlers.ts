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
    const entries = [
      {
        id: 1,
        timestamp_ms: Date.now() - 10000,
        level: "info" as const,
        tag: "wifi",
        message: "Wi-Fi initialized",
      },
      {
        id: 2,
        timestamp_ms: Date.now() - 8000,
        level: "info" as const,
        tag: "mdns",
        message: "mDNS service started",
      },
      {
        id: 3,
        timestamp_ms: Date.now() - 5000,
        level: "warn" as const,
        tag: "wifi",
        message: "STA not configured",
      },
      {
        id: 4,
        timestamp_ms: Date.now() - 2000,
        level: "info" as const,
        tag: "http",
        message: "HTTP server started on port 80",
      },
      {
        id: 5,
        timestamp_ms: Date.now() - 500,
        level: "debug" as const,
        tag: "system",
        message: "Heap usage: 35%",
      },
    ];

    return HttpResponse.json({
      status: "success",
      data: {
        entries,
        next_cursor: 6,
        has_more: false,
      },
    });
  }),
];
