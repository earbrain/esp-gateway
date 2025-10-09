import { useApi } from "../../hooks/useApi";
import { usePolling } from "../../hooks/usePolling";
import type { FunctionalComponent } from "preact";
import { useEffect } from "preact/hooks";

type DeviceLogsPageProps = {
  path?: string;
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

export const DeviceLogsPage: FunctionalComponent<DeviceLogsPageProps> = () => {
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

  const entries = logs?.entries ?? [];
  const hasEntries = entries.length > 0;
  const showLoadingPlaceholder = logsLoading && !hasEntries;

  useEffect(() => {
    fetchLogs();
  }, [fetchLogs]);

  return (
    <section class="space-y-6">
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
          <div class="flex flex-col overflow-hidden rounded border border-slate-200" style="height: calc(100vh - 20rem);">
            <div class="flex-1 overflow-x-auto overflow-y-auto">
              <table class="w-full min-w-[560px] table-auto text-left text-sm text-slate-700">
                <thead class="sticky top-0 bg-white">
                  <tr class="text-xs uppercase tracking-wide text-slate-500 border-b border-slate-200">
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
              <p class="border-t border-slate-200 px-3 py-2 text-xs text-slate-500 bg-white">
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
