import { useEffect, useMemo } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../hooks/useApi";
import type { WifiStatus } from "../types/wifi";

const Spinner = () => (
  <span
    class="inline-block h-4 w-4 animate-spin rounded-full border-2 border-slate-300 border-t-sky-500"
    aria-hidden="true"
  ></span>
);

type WifiStatusCardProps = {
  refresh?: number;
};

export const WifiStatusCard: FunctionalComponent<WifiStatusCardProps> = ({ refresh }) => {
  const {
    data: status,
    loading: statusLoading,
    execute: fetchStatus,
  } = useApi<WifiStatus>("/api/v1/wifi/status", {
    method: "GET",
  });

  useEffect(() => {
    fetchStatus();
  }, [refresh]);

  const statusSummary = useMemo(() => {
    if (!status) {
      if (statusLoading) {
        return "Checking Wi-Fi status...";
      }
      return "Status unavailable";
    }
    if (status.sta_connected) {
      return status.ip ? `Connected to ${status.ip}` : "Connected";
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return `Connection error: ${status.sta_error}`;
    }
    return "Not connected";
  }, [status, statusLoading]);

  const apState = useMemo(() => {
    if (!status) {
      return {
        label: "Unknown",
        description: "Waiting for status update.",
        badgeClass: "status-pill bg-slate-200 text-slate-600",
      };
    }
    if (status.ap_active) {
      return {
        label: "Active",
        description: "Device is broadcasting the setup access point.",
        badgeClass: "status-pill bg-emerald-100 text-emerald-700",
      };
    }
    return {
      label: "Inactive",
      description: "Soft AP is currently disabled.",
      badgeClass: "status-pill bg-slate-200 text-slate-600",
    };
  }, [status]);

  const staState = useMemo(() => {
    if (!status) {
      return {
        label: statusLoading ? "Checking" : "Unknown",
        description: statusLoading
          ? "Fetching the latest station status..."
          : "Waiting for status update.",
        badgeClass: statusLoading
          ? "status-pill bg-sky-100 text-sky-600"
          : "status-pill bg-slate-200 text-slate-600",
      };
    }
    if (status.sta_connected) {
      return {
        label: "Connected",
        description: status.ip
          ? `Client connected with IP ${status.ip}.`
          : "Client connected to Wi-Fi network.",
        badgeClass: "status-pill bg-emerald-100 text-emerald-700",
      };
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return {
        label: "Error",
        description: `Last error: ${status.sta_error}.`,
        badgeClass: "status-pill bg-rose-100 text-rose-700",
      };
    }
    return {
      label: "Not connected",
      description: "STA client is idle.",
      badgeClass: "status-pill bg-slate-200 text-slate-600",
    };
  }, [status, statusLoading]);

  return (
    <div class="card">
      <div class="flex items-center justify-between gap-3">
        <h2 class="section-title !mb-0">Wi-Fi Status</h2>
        {statusLoading && <Spinner />}
      </div>
      <p class="text-sm text-slate-600">{statusSummary}</p>
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
        <button type="button" class="btn-secondary" onClick={() => fetchStatus()}>
          Refresh
        </button>
      </div>
    </div>
  );
};
