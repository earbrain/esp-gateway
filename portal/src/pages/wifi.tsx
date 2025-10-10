import { useCallback, useEffect, useMemo, useRef, useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../hooks/useApi";
import { usePolling } from "../hooks/usePolling";
import { Toast } from "../components/Toast";
import { WifiStatusCard } from "../components/WifiStatusCard";
import { WifiNetworkCard } from "../components/wifi/WifiNetworkCard";
import type { WifiNetwork, WifiScanResponse, WifiStatus } from "../types/wifi";

type WifiPageProps = {
  path?: string;
};

const prioritizeConnected = (list: WifiNetwork[]): WifiNetwork[] => {
  const connectedIndex = list.findIndex((network) => network.connected);
  if (connectedIndex <= 0) {
    return list;
  }
  const connected = list[connectedIndex];
  return [
    connected,
    ...list.slice(0, connectedIndex),
    ...list.slice(connectedIndex + 1),
  ];
};

// Loading spinner
const LoadingSpinner: FunctionalComponent<{ label?: string }> = ({ label }) => (
  <div class="mt-4 flex items-center gap-2 text-sm text-slate-500" role="status" aria-live="polite">
    <span class="inline-block h-4 w-4 animate-spin rounded-full border-2 border-slate-300 border-t-sky-500"></span>
    <span>{label ?? "Loading..."}</span>
  </div>
);

export const WifiPage: FunctionalComponent<WifiPageProps> = () => {
  const [networks, setNetworks] = useState<WifiNetwork[]>([]);
  const [toast, setToast] = useState<{ message: string; type: "success" | "error" } | null>(null);
  const [isScanning, setIsScanning] = useState(false);
  const [hasScanned, setHasScanned] = useState(false);

  const {
    data: status,
    loading: statusLoading,
    error: statusError,
    execute: fetchStatus,
  } = useApi<WifiStatus>("/api/v1/wifi/status", {
    method: "GET",
  });

  const {
    data: scanResult,
    error: scanError,
    execute: executeScan,
  } = useApi<WifiScanResponse>("/api/v1/wifi/scan", {
    method: "GET",
  });

  const { refresh: refreshStatus } = usePolling(() => fetchStatus(), {
    intervalMs: 5000,
  });

  const lastStatusError = useRef<string | null>(null);
  const lastScanError = useRef<string | null>(null);

  const showToast = useCallback((type: "success" | "error", message: string) => {
    setToast({ type, message });
  }, []);

  const triggerScan = useCallback(async () => {
    setIsScanning(true);
    try {
      await executeScan({ url: "/api/v1/wifi/scan" });
    } catch (err) {
      console.error(err);
    } finally {
      setIsScanning(false);
    }
  }, [executeScan]);

  useEffect(() => {
    if (statusError && statusError !== lastStatusError.current) {
      showToast("error", statusError);
      lastStatusError.current = statusError;
    }
    if (!statusError) {
      lastStatusError.current = null;
    }
  }, [showToast, statusError]);

  useEffect(() => {
    if (scanError && scanError !== lastScanError.current) {
      showToast("error", scanError);
      lastScanError.current = scanError;
    }
    if (!scanError) {
      lastScanError.current = null;
    }
  }, [scanError, showToast]);

  useEffect(() => {
    if (!scanResult) {
      return;
    }

    setHasScanned(true);

    if (scanResult.error && scanResult.error.length > 0) {
      return;
    }

    setNetworks((prev) => {
      const previous = new Map<string, WifiNetwork>(prev.map((item) => [item.id, item]));
      const next = scanResult.networks
        .filter((network) => network.ssid.trim().length > 0)
        .map((network) => {
          const normalizedBssid = network.bssid.trim().toLowerCase();
          const id = normalizedBssid.length > 0 ? normalizedBssid : `${network.ssid}-${network.channel}`;
          const existing = previous.get(id);
          return {
            id,
            ssid: network.ssid,
            signal: Math.max(0, Math.min(100, Math.round(network.signal))),
            security: network.security,
            connected: network.connected,
            hidden: network.hidden,
            bssid: network.bssid,
            rssi: network.rssi,
            lastPassphrase: existing?.lastPassphrase ?? "",
          };
        });
      return prioritizeConnected(next);
    });
  }, [scanResult]);

  const statusSummary = useMemo(() => {
    if (!status) {
      if (statusLoading) {
        return "Checking Wi-Fi status...";
      }
      return "Status unavailable";
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return `Connection error: ${status.sta_error}`;
    }
    if (status.sta_connected) {
      return status.ip ? `Connected to ${status.ip}` : "Connected";
    }
    if (status.sta_connecting) {
      return "Connecting to network...";
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
    if (status.sta_connecting) {
      return {
        label: "Connecting",
        description: "Attempting to join the configured network...",
        badgeClass: "status-pill bg-amber-100 text-amber-700",
      };
    }
    if (status.sta_error && status.sta_error.length > 0) {
      return {
        label: "Error",
        description: `Last error: ${status.sta_error}.`,
        badgeClass: "status-pill bg-rose-100 text-rose-700",
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
    return {
      label: "Not connected",
      description: "STA client is idle.",
      badgeClass: "status-pill bg-slate-200 text-slate-600",
    };
  }, [status, statusLoading]);

  const handleScan = () => {
    if (!isScanning) {
      void triggerScan();
    }
  };

  const handlePasswordSaved = useCallback((networkId: string, password: string) => {
    setNetworks((prev) =>
      prev.map((network) =>
        network.id === networkId
          ? { ...network, lastPassphrase: password }
          : network
      )
    );
  }, []);

  return (
    <>
      {toast && (
        <div
          class="fixed inset-x-0 top-4 z-40 flex justify-center px-4"
          role="status"
          aria-live="polite"
        >
          <Toast message={toast.message} type={toast.type} onClose={() => setToast(null)} />
        </div>
      )}
      <section class="space-y-6">
        <WifiStatusCard
          apState={apState}
          staState={staState}
          status={status}
          loading={statusLoading}
          onRefresh={refreshStatus}
          summary={statusSummary}
        />
        <div class="card">
          <div class="flex items-center justify-between">
            <div>
              <h2 class="section-title !mb-1">Available Networks</h2>
              <p class="text-sm text-slate-500">
                Choose a nearby Wi-Fi network and enter the password if required.
              </p>
            </div>
            <button
              type="button"
              class="btn-secondary"
              onClick={handleScan}
              disabled={isScanning}
            >
              {isScanning ? "Scanning..." : hasScanned ? "Rescan" : "Scan"}
            </button>
          </div>
          {isScanning && <LoadingSpinner label="Scanning for networks..." />}
          <div class="mt-5 grid gap-2">
            {networks.map((network) => (
              <WifiNetworkCard
                key={network.id}
                network={network}
                onPasswordSaved={handlePasswordSaved}
              />
            ))}
          </div>
          {networks.length === 0 && (
            <div class="mt-6 rounded-2xl border border-dashed border-slate-300 bg-slate-50 p-6 text-center text-sm text-slate-500">
              {isScanning
                ? "Scanning nearby networks..."
                : hasScanned
                  ? "No networks found. Try scanning again."
                  : "Press Scan to search for nearby networks."}
            </div>
          )}
        </div>
      </section>
    </>
  );
};
