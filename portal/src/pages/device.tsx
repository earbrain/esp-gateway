import { useApi } from "../hooks/useApi";
import { usePolling } from "../hooks/usePolling";
import type { FunctionalComponent } from "preact";
import { useEffect } from "preact/hooks";

type DevicePageProps = {
  path?: string;
};

type DeviceInfo = {
  model: string;
  firmware_version: string;
  build_time: string;
  idf_version: string;
};

type Metrics = {
  heap_total: number;
  heap_free: number;
  heap_used: number;
  heap_min_free: number;
  heap_largest_free_block: number;
  timestamp_ms: number;
};

type MdnsDetail = {
  hostname: string;
  instance_name: string;
  service_type: string;
  protocol: string;
  port: number;
  running: boolean;
};

type LogLevel = "none" | "error" | "warn" | "info" | "debug" | "verbose";

type LogEntry = {
  id: number;
  timestamp_ms: number;
  level: LogLevel;
  tag: string;
  message: string;
};

type LogResponse = {
  entries: LogEntry[];
  next_cursor: number;
  has_more: boolean;
};

export const DevicePage: FunctionalComponent<DevicePageProps> = () => {
  const { data: info, loading, error, execute } = useApi<DeviceInfo>("/api/v1/device-info");
  const {
    data: metrics,
    loading: metricsLoading,
    error: metricsError,
    execute: fetchMetrics,
  } = useApi<Metrics>("/api/v1/metrics");
  const {
    data: mdnsDetail,
    loading: mdnsLoading,
    error: mdnsError,
    execute: fetchMdns,
  } = useApi<MdnsDetail>("/api/v1/mdns");
  const {
    data: logs,
    loading: logsLoading,
    error: logsError,
    execute: fetchLogs,
  } = useApi<LogResponse>("/api/v1/logs?limit=1000");
  const { refresh: refreshLogs } = usePolling(() => fetchLogs(), {
    intervalMs: 5000,
    immediate: false,
  });
  const { refresh: refreshMetrics } = usePolling(() => fetchMetrics(), {
    intervalMs: 5000,
    immediate: false,
  });

  const entries = logs?.entries ?? [];
  const hasEntries = entries.length > 0;
  const showLoadingPlaceholder = logsLoading && !hasEntries;

  const formatBytes = (value: number): string => {
    if (!Number.isFinite(value) || value < 0) {
      return "-";
    }
    if (value < 1024) {
      return `${value} B`;
    }
    if (value < 1024 * 1024) {
      return `${(value / 1024).toFixed(1)} KiB`;
    }
    return `${(value / (1024 * 1024)).toFixed(2)} MiB`;
  };

  const heapUsagePercent =
    metrics && metrics.heap_total > 0
      ? Math.round((metrics.heap_used / metrics.heap_total) * 100)
      : 0;
  const showMetricsLoading = metricsLoading && !metrics;
  useEffect(() => {
    execute();
    fetchMetrics();
    fetchMdns();
    fetchLogs();
  }, [execute, fetchMetrics, fetchMdns, fetchLogs]);

  return (
    <section class="space-y-6">
      <div class="card">
        <h2 class="section-title">Device Overview</h2>
        {loading && <p class="muted">Loading...</p>}
        {error && <p class="error-text">{error}</p>}
        {info && (
          <dl class="grid gap-3 text-sm text-slate-700">
            <div class="info-row">
              <dt>Model</dt>
              <dd>{info.model}</dd>
            </div>
            <div class="info-row">
              <dt>Firmware</dt>
              <dd>{info.firmware_version}</dd>
            </div>
            <div class="info-row">
              <dt>Build Time</dt>
              <dd>{info.build_time}</dd>
            </div>
            <div class="info-row">
              <dt>ESP-IDF</dt>
              <dd>{info.idf_version}</dd>
            </div>
          </dl>
        )}
      </div>
      <div class="card space-y-4">
        <div class="flex items-center justify-between gap-4">
          <h2 class="section-title">System Metrics</h2>
          <button
            type="button"
            class="btn-secondary"
            onClick={() => {
              void refreshMetrics();
            }}
            disabled={metricsLoading}
          >
            Refresh
          </button>
        </div>
        {showMetricsLoading && <p class="muted">Loading...</p>}
        {metricsError && <p class="error-text">{metricsError}</p>}
        {metrics && (
          <div class="space-y-5 text-sm text-slate-700">
            <div>
              <h3 class="text-xs font-semibold uppercase tracking-wide text-slate-500">Heap</h3>
              <dl class="mt-2 grid gap-3">
                <div class="info-row">
                  <dt>Total Heap</dt>
                  <dd>{formatBytes(metrics.heap_total)}</dd>
                </div>
                <div class="info-row">
                  <dt>Heap Used</dt>
                  <dd>
                    {formatBytes(metrics.heap_used)}{" "}
                    <span class="muted text-xs">({heapUsagePercent}%)</span>
                  </dd>
                </div>
                <div class="info-row">
                  <dt>Heap Free</dt>
                  <dd>{formatBytes(metrics.heap_free)}</dd>
                </div>
                <div class="info-row">
                  <dt>Minimum Free (Watermark)</dt>
                  <dd>{formatBytes(metrics.heap_min_free)}</dd>
                </div>
                <div class="info-row">
                  <dt>Largest Free Block</dt>
                  <dd>{formatBytes(metrics.heap_largest_free_block)}</dd>
                </div>
              </dl>
            </div>
          </div>
        )}
      </div>
      <div class="card">
        <h2 class="section-title">mDNS</h2>
        {mdnsLoading && <p class="muted">Loading...</p>}
        {mdnsError && <p class="error-text">{mdnsError}</p>}
        {mdnsDetail && (
          <dl class="grid gap-3 text-sm text-slate-700">
            <div class="info-row">
              <dt>Status</dt>
              <dd>
                <span
                  class={`status-pill ${
                    mdnsDetail.running
                      ? "bg-emerald-100 text-emerald-700"
                      : "bg-slate-200 text-slate-600"
                  }`}
                >
                  {mdnsDetail.running ? "Active" : "Stopped"}
                </span>
              </dd>
            </div>
            <div class="info-row">
              <dt>Hostname</dt>
              <dd>{mdnsDetail.hostname}</dd>
            </div>
            <div class="info-row">
              <dt>Instance</dt>
              <dd>{mdnsDetail.instance_name}</dd>
            </div>
            <div class="info-row">
              <dt>Service</dt>
              <dd>{mdnsDetail.service_type}</dd>
            </div>
            <div class="info-row">
              <dt>Protocol</dt>
              <dd>{mdnsDetail.protocol}</dd>
            </div>
            <div class="info-row">
              <dt>Port</dt>
              <dd>{mdnsDetail.port}</dd>
            </div>
            <div class="info-row">
              <dt>Access URL</dt>
              <dd>
                <a
                  href={`http://${mdnsDetail.hostname}.local/`}
                  target="_blank"
                  rel="noopener noreferrer"
                  class="text-sky-600 underline decoration-sky-400 decoration-2 underline-offset-4 hover:text-sky-700"
                >
                  {`http://${mdnsDetail.hostname}.local/`}
                </a>
              </dd>
            </div>
          </dl>
        )}
      </div>
      <div class="card space-y-4">
        <div class="flex items-center justify-between gap-4">
          <h2 class="section-title">Device Logs</h2>
          <div class="flex items-center gap-3">
            {logsLoading && hasEntries && <span class="text-xs text-slate-500">Updating...</span>}
            <button
              type="button"
              class="btn-secondary"
              onClick={() => {
                void refreshLogs();
              }}
              disabled={logsLoading}
            >
              Refresh
            </button>
          </div>
        </div>
        {showLoadingPlaceholder && (
          <div class="flex items-center gap-2 text-slate-500">
            <svg
              class="h-4 w-4 animate-spin"
              viewBox="0 0 24 24"
              fill="none"
              stroke="currentColor"
              stroke-width="2"
              stroke-linecap="round"
              stroke-linejoin="round"
              aria-label="Loading logs"
            >
              <path d="M21 12a9 9 0 1 1-6.219-8.56"></path>
              <path d="M21 12a9 9 0 0 0-9-9"></path>
            </svg>
          </div>
        )}
        {logsError && <p class="error-text">{logsError}</p>}
        {hasEntries ? (
          <div class="overflow-hidden rounded border border-slate-200">
            <div class="max-h-96 overflow-x-auto overflow-y-auto">
              <table class="w-full min-w-[560px] table-auto text-left text-sm text-slate-700">
                <thead>
                  <tr class="text-xs uppercase tracking-wide text-slate-500">
                    <th class="px-3 py-2">Time (ms)</th>
                    <th class="px-3 py-2">Level</th>
                    <th class="px-3 py-2">Tag</th>
                    <th class="px-3 py-2">Message</th>
                  </tr>
                </thead>
                <tbody class="divide-y divide-slate-200">
                  {entries.map((entry) => (
                    <tr key={entry.id} class="align-top">
                      <td class="px-3 py-2 font-mono text-xs text-slate-500">
                        {entry.timestamp_ms}
                      </td>
                      <td class="px-3 py-2">
                        <span
                          class={`status-pill ${
                            entry.level === "error"
                              ? "bg-rose-100 text-rose-700"
                              : entry.level === "warn"
                                ? "bg-amber-100 text-amber-700"
                                : entry.level === "info"
                                  ? "bg-sky-100 text-sky-700"
                                  : entry.level === "debug"
                                    ? "bg-slate-200 text-slate-600"
                                    : "bg-slate-200 text-slate-600"
                          }`}
                        >
                          {entry.level.toUpperCase()}
                        </span>
                      </td>
                      <td class="px-3 py-2 font-mono text-xs text-slate-500">{entry.tag || "-"}</td>
                      <td class="px-3 py-2 whitespace-pre-wrap break-words">{entry.message}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            {logs?.has_more && (
              <p class="border-t border-slate-200 px-3 py-2 text-xs text-slate-500">
                Showing latest {entries.length} entries. More logs are available on the device.
              </p>
            )}
          </div>
        ) : (
          !logsLoading && (
            <div class="rounded border border-dashed border-slate-300 bg-slate-50 px-4 py-8 text-center text-sm text-slate-500">
              Logs will appear here once the device emits output.
            </div>
          )
        )}
      </div>
    </section>
  );
};
