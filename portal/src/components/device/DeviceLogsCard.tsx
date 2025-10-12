import { useEffect } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useTranslation } from "../../i18n/context";
import { useApi } from "../../hooks/useApi";
import { usePolling } from "../../hooks/usePolling";
import type { LogResponse } from "../../types/device";

export const DeviceLogsCard: FunctionalComponent = () => {
  const t = useTranslation();
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

  useEffect(() => {
    fetchLogs();
  }, [fetchLogs]);

  const entries = logs?.entries ?? [];
  const hasEntries = entries.length > 0;
  const showLoadingPlaceholder = logsLoading && !hasEntries;

  return (
    <div class="card space-y-4">
      <div class="flex items-center justify-between gap-4">
        <h2 class="section-title">{t("page.logs.title")}</h2>
        <div class="flex items-center gap-3">
          {logsLoading && hasEntries && (
            <span class="text-xs text-slate-500">{t("common.updating")}</span>
          )}
          <button
            type="button"
            class="btn-secondary"
            onClick={() => {
              void refreshLogs();
            }}
            disabled={logsLoading}
          >
            {t("common.refresh")}
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
            aria-label={t("logs.loadingAria")}
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
                  <th class="px-3 py-2">{t("logs.columns.time")}</th>
                  <th class="px-3 py-2">{t("logs.columns.level")}</th>
                  <th class="px-3 py-2">{t("logs.columns.tag")}</th>
                  <th class="px-3 py-2">{t("logs.columns.message")}</th>
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
              {t("logs.moreEntries", { count: entries.length })}
            </p>
          )}
        </div>
      ) : (
        !logsLoading && (
          <div class="rounded border border-dashed border-slate-300 bg-slate-50 px-4 py-8 text-center text-sm text-slate-500">
            {t("logs.emptyState")}
          </div>
        )
      )}
    </div>
  );
};
