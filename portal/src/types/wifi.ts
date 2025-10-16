export type WifiResponse = {
  restart_required?: boolean;
  sta_connect_started?: boolean;
  sta_error?: string;
};

export type WifiStatus = {
  ap_active: boolean;
  sta_active: boolean;
  sta_connected: boolean;
  sta_connecting: boolean;
  sta_error: string;
  ip: string;
  disconnect_reason: number;
};

export type WifiScanNetwork = {
  ssid: string;
  bssid: string;
  signal: number;
  security: string;
  connected: boolean;
  hidden: boolean;
  rssi: number;
  channel: number;
};

export type WifiScanResponse = {
  networks: WifiScanNetwork[];
  error: string;
};

export type WifiNetwork = {
  id: string;
  ssid: string;
  signal: number;
  security: string;
  connected: boolean;
  hidden: boolean;
  bssid: string;
  rssi: number;
};
