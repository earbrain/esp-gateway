import { useEffect } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../../hooks/useApi";
import type { MdnsDetail } from "../../types/device";

export const MdnsCard: FunctionalComponent = () => {
  const {
    data: mdnsDetail,
    loading: mdnsLoading,
    error: mdnsError,
    execute: fetchMdns,
  } = useApi<MdnsDetail>("/api/v1/mdns");

  useEffect(() => {
    fetchMdns();
  }, [fetchMdns]);

  return (
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
  );
};
