import { useEffect } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useTranslation } from "../../i18n/context";
import { useApi } from "../../hooks/useApi";
import type { DeviceDetail } from "../../types/device";

export const DeviceOverviewCard: FunctionalComponent = () => {
  const t = useTranslation();
  const { data: info, loading, error, execute } = useApi<DeviceDetail>("/api/v1/device");

  useEffect(() => {
    execute();
  }, [execute]);

  return (
    <div class="card">
      <h2 class="section-title">{t("device.overview.title")}</h2>
      {loading && <p class="muted">{t("common.loading")}</p>}
      {error && <p class="error-text">{error}</p>}
      {info && (
        <dl class="grid gap-3 text-sm text-slate-700">
          <div class="info-row">
            <dt>{t("device.overview.model")}</dt>
            <dd>{info.model}</dd>
          </div>
          <div class="info-row">
            <dt>{t("device.overview.gatewayVersion")}</dt>
            <dd>{info.gateway_version}</dd>
          </div>
          <div class="info-row">
            <dt>{t("device.overview.buildTime")}</dt>
            <dd>{info.build_time}</dd>
          </div>
          <div class="info-row">
            <dt>{t("device.overview.idfVersion")}</dt>
            <dd>{info.idf_version}</dd>
          </div>
        </dl>
      )}
    </div>
  );
};
