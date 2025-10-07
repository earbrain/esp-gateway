import { memo } from "preact/compat";
import type { FunctionalComponent } from "preact";

type WifiStatusCardProps = {
  apState: {
    label: string;
    description: string;
    badgeClass: string;
  };
  staState: {
    label: string;
    description: string;
    badgeClass: string;
  };
  status: {
    ap_active: boolean;
    sta_active: boolean;
    sta_connecting: boolean;
    sta_connected: boolean;
    sta_error: string;
    ip: string;
    disconnect_reason: number;
  } | null;
  loading: boolean;
  onRefresh: () => void;
  summary: string;
};

const Spinner = () => (
  <span
    class="inline-block h-4 w-4 animate-spin rounded-full border-2 border-slate-300 border-t-sky-500"
    aria-hidden="true"
  ></span>
);

export const WifiStatusCard: FunctionalComponent<WifiStatusCardProps> = memo(
  ({ apState, staState, status, loading, onRefresh, summary }) => (
    <div class="card">
      <div class="flex items-center justify-between gap-3">
        <h2 class="section-title !mb-0">Wi-Fi Status</h2>
        {loading && <Spinner />}
      </div>
      <p class="text-sm text-slate-600">{summary}</p>
      <div class="mt-4 grid gap-4 md:grid-cols-2">
        <div class="rounded-xl border border-slate-200 bg-slate-50 p-4">
          <div class="flex items-center justify-between">
            <h3 class="text-sm font-semibold text-slate-800">Access Point</h3>
            <span class={apState.badgeClass}>{apState.label}</span>
          </div>
          <p class="mt-2 text-sm text-slate-600">{apState.description}</p>
          {status && (
            <dl class="mt-3 space-y-2 text-sm text-slate-700">
              <div class="info-row">
                <dt>Broadcasting</dt>
                <dd>{status.ap_active ? "Yes" : "No"}</dd>
              </div>
            </dl>
          )}
        </div>
        <div class="rounded-xl border border-slate-200 bg-slate-50 p-4">
          <div class="flex items-center justify-between">
            <h3 class="text-sm font-semibold text-slate-800">Station</h3>
            <span class={staState.badgeClass}>{staState.label}</span>
          </div>
          <p class="mt-2 text-sm text-slate-600">{staState.description}</p>
          {status && (
            <dl class="mt-3 space-y-2 text-sm text-slate-700">
              <div class="info-row">
                <dt>Enabled</dt>
                <dd>{status.sta_active ? "Yes" : "No"}</dd>
              </div>
              <div class="info-row">
                <dt>IP Address</dt>
                <dd>{status.ip || "-"}</dd>
              </div>
              <div class="info-row">
                <dt>Disconnect Reason</dt>
                <dd>{status.disconnect_reason}</dd>
              </div>
              <div class="info-row">
                <dt>Last Error</dt>
                <dd>{status.sta_error || "None"}</dd>
              </div>
            </dl>
          )}
        </div>
      </div>
      <div class="mt-4 flex justify-end">
        <button type="button" class="btn-secondary" onClick={onRefresh}>
          Refresh
        </button>
      </div>
    </div>
  ),
);

WifiStatusCard.displayName = "WifiStatusCard";

