import { useTranslation } from "../i18n/context";
import type { TranslationKey } from "../i18n/translations";

type ConnectionType = "ap" | "sta" | "apsta" | "unknown";

type ConnectionBadgeProps = {
  type: ConnectionType;
};

const connectionLabelKeys: Record<ConnectionType, TranslationKey> = {
  ap: "connection.badge.ap",
  sta: "connection.badge.sta",
  apsta: "connection.badge.apsta",
  unknown: "connection.badge.unknown",
};

const connectionColors: Record<ConnectionType, string> = {
  ap: "bg-amber-100 text-amber-800 border-amber-300",
  sta: "bg-green-100 text-green-800 border-green-300",
  apsta: "bg-blue-100 text-blue-800 border-blue-300",
  unknown: "bg-slate-100 text-slate-600 border-slate-300",
};

export function ConnectionBadge({ type }: ConnectionBadgeProps) {
  const t = useTranslation();
  const label = t(connectionLabelKeys[type]);
  const colorClass = connectionColors[type];

  return (
    <div
      class={`inline-flex items-center rounded-full border px-3 py-1 text-xs font-medium ${colorClass}`}
      title={label}
    >
      {label}
    </div>
  );
}
