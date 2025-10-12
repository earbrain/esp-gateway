import type { FunctionalComponent } from "preact";
import { useTranslation } from "../../i18n/context";

type SecurityBadgeProps = {
  security: string;
};

export const SecurityBadge: FunctionalComponent<SecurityBadgeProps> = ({ security }) => {
  const t = useTranslation();
  const label = security === "Open" ? t("wifi.security.none") : security;

  return (
    <span class="rounded-full border border-slate-200 px-2 py-0.5 text-xs font-medium text-slate-500">
      {label}
    </span>
  );
};
