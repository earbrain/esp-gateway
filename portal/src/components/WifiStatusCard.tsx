import { useEffect, useMemo } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useTranslation } from "../i18n/context";
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
  const t = useTranslation();
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
        return t("wifi.status.summary.checking");
      }
      return t("wifi.status.summary.unavailable");
    }
    if (status.sta_connected) {
      return status.ip
        ? t("wifi.status.summary.connectedWithIp", { ip: status.ip })
        : t("wifi.status.summary.connected");
    }
    if (status.sta_connecting) {
      return t("wifi.status.summary.connecting");
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return t("wifi.status.summary.connectionError", { message: status.sta_error });
    }
    return t("wifi.status.summary.notConnected");
  }, [status, statusLoading, t]);

  const apState = useMemo(() => {
    if (!status) {
      return {
        label: t("common.unknown"),
        description: t("wifi.status.ap.descriptionUnknown"),
        badgeClass: "status-pill bg-slate-200 text-slate-600",
      };
    }
    if (status.ap_active) {
      return {
        label: t("wifi.status.ap.active"),
        description: t("wifi.status.ap.descriptionActive"),
        badgeClass: "status-pill bg-emerald-100 text-emerald-700",
      };
    }
    return {
      label: t("wifi.status.ap.inactive"),
      description: t("wifi.status.ap.descriptionInactive"),
      badgeClass: "status-pill bg-slate-200 text-slate-600",
    };
  }, [status, t]);

  const staState = useMemo(() => {
    if (!status) {
      return {
        label: statusLoading ? t("wifi.status.sta.checking") : t("common.unknown"),
        description: statusLoading
          ? t("wifi.status.sta.descriptionChecking")
          : t("wifi.status.ap.descriptionUnknown"),
        badgeClass: statusLoading
          ? "status-pill bg-sky-100 text-sky-600"
          : "status-pill bg-slate-200 text-slate-600",
      };
    }
    if (status.sta_connected) {
      return {
        label: t("wifi.status.sta.connected"),
        description: status.ip
          ? t("wifi.status.sta.descriptionConnectedWithIp", { ip: status.ip })
          : t("wifi.status.sta.descriptionConnected"),
        badgeClass: "status-pill bg-emerald-100 text-emerald-700",
      };
    }
    if (status.sta_connecting) {
      return {
        label: t("wifi.status.sta.connecting"),
        description: t("wifi.status.sta.descriptionConnecting"),
        badgeClass: "status-pill bg-sky-100 text-sky-600",
      };
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return {
        label: t("wifi.status.sta.error"),
        description: t("wifi.status.sta.descriptionError", { message: status.sta_error }),
        badgeClass: "status-pill bg-rose-100 text-rose-700",
      };
    }
    return {
      label: t("wifi.status.sta.notConnected"),
      description: t("wifi.status.sta.descriptionIdle"),
      badgeClass: "status-pill bg-slate-200 text-slate-600",
    };
  }, [status, statusLoading, t]);

  return (
    <div class="card">
      <div class="flex items-center justify-between gap-3">
        <h2 class="section-title !mb-0">{t("wifi.status.title")}</h2>
        {statusLoading && <Spinner />}
      </div>
      <p class="text-sm text-slate-600">{statusSummary}</p>
      <div class="mt-4 grid gap-4 md:grid-cols-2">
        <div class="rounded-xl border border-slate-200 bg-slate-50 p-4">
          <div class="flex items-center justify-between">
            <h3 class="text-sm font-semibold text-slate-800">{t("wifi.status.ap.title")}</h3>
            <span class={apState.badgeClass}>{apState.label}</span>
          </div>
          <p class="mt-2 text-sm text-slate-600">{apState.description}</p>
          {status && (
            <dl class="mt-3 space-y-2 text-sm text-slate-700">
              <div class="info-row">
                <dt>{t("wifi.status.fields.broadcasting")}</dt>
                <dd>{status.ap_active ? t("common.yes") : t("common.no")}</dd>
              </div>
            </dl>
          )}
        </div>
        <div class="rounded-xl border border-slate-200 bg-slate-50 p-4">
          <div class="flex items-center justify-between">
            <h3 class="text-sm font-semibold text-slate-800">{t("wifi.status.sta.title")}</h3>
            <span class={staState.badgeClass}>{staState.label}</span>
          </div>
          <p class="mt-2 text-sm text-slate-600">{staState.description}</p>
          {status && (
            <dl class="mt-3 space-y-2 text-sm text-slate-700">
              <div class="info-row">
                <dt>{t("wifi.status.fields.enabled")}</dt>
                <dd>{status.sta_active ? t("common.yes") : t("common.no")}</dd>
              </div>
              <div class="info-row">
                <dt>{t("wifi.status.fields.ipAddress")}</dt>
                <dd>{status.ip || "-"}</dd>
              </div>
              <div class="info-row">
                <dt>{t("wifi.status.fields.disconnectReason")}</dt>
                <dd>{status.disconnect_reason}</dd>
              </div>
              <div class="info-row">
                <dt>{t("wifi.status.fields.lastError")}</dt>
                <dd>{status.sta_error || t("common.none")}</dd>
              </div>
            </dl>
          )}
        </div>
      </div>
      <div class="mt-4 flex justify-end">
        <button type="button" class="btn-secondary" onClick={() => fetchStatus()}>
          {t("common.refresh")}
        </button>
      </div>
    </div>
  );
};
