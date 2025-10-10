import type { FunctionalComponent } from "preact";

type SecurityBadgeProps = {
  security: string;
};

export const SecurityBadge: FunctionalComponent<SecurityBadgeProps> = ({ security }) => (
  <span class="rounded-full border border-slate-200 px-2 py-0.5 text-xs font-medium text-slate-500">
    {security === "Open" ? "No security" : security}
  </span>
);
