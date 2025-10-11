export type DeviceDetail = {
  model: string;
  firmware_version: string;
  build_time: string;
  idf_version: string;
};

export type HealthStatus = {
  status: string;
  uptime: number;
  version: string;
  connection_type: "ap" | "sta" | "apsta" | "unknown";
};

export type Metrics = {
  heap_total: number;
  heap_free: number;
  heap_used: number;
  heap_min_free: number;
  heap_largest_free_block: number;
  timestamp_ms: number;
};

export type MdnsDetail = {
  hostname: string;
  instance_name: string;
  service_type: string;
  protocol: string;
  port: number;
  running: boolean;
};

export type LogLevel = "none" | "error" | "warn" | "info" | "debug" | "verbose";

export type LogEntry = {
  id: number;
  timestamp_ms: number;
  level: LogLevel;
  tag: string;
  message: string;
};

export type LogResponse = {
  entries: LogEntry[];
  next_cursor: number;
  has_more: boolean;
};
