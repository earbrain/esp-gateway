import { useCallback, useEffect, useRef, useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../../hooks/useApi";
import { WifiNetworkCard } from "./WifiNetworkCard";
import type { WifiNetwork, WifiScanResponse } from "../../types/wifi";

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

const LoadingSpinner: FunctionalComponent<{ label?: string }> = ({ label }) => (
  <div class="mt-4 flex items-center gap-2 text-sm text-slate-500" role="status" aria-live="polite">
    <span class="inline-block h-4 w-4 animate-spin rounded-full border-2 border-slate-300 border-t-sky-500"></span>
    <span>{label ?? "Loading..."}</span>
  </div>
);

type WifiNetworkListProps = {
  onError?: (message: string) => void;
};

export const WifiNetworkList: FunctionalComponent<WifiNetworkListProps> = ({ onError }) => {
  const [networks, setNetworks] = useState<WifiNetwork[]>([]);
  const [isScanning, setIsScanning] = useState(false);
  const [hasScanned, setHasScanned] = useState(false);

  const {
    data: scanResult,
    error: scanError,
    execute: executeScan,
  } = useApi<WifiScanResponse>("/api/v1/wifi/scan", {
    method: "GET",
  });

  const lastScanError = useRef<string | null>(null);

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
    if (scanError && scanError !== lastScanError.current) {
      if (onError) {
        onError(scanError);
      }
      lastScanError.current = scanError;
    }
    if (!scanError) {
      lastScanError.current = null;
    }
  }, [scanError, onError]);

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
  );
};
