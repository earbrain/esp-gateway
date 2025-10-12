import type { FunctionalComponent } from "preact";
import { useTranslation } from "../i18n/context";
import type { TranslationKey } from "../i18n/translations";

export type Breadcrumb = {
  labelKey: TranslationKey;
  path?: string;
};

export type PageMeta = {
  showHeader: boolean;
  titleKey: TranslationKey;
  descriptionKey: TranslationKey;
  breadcrumbs: Breadcrumb[];
};

type BreadcrumbSectionProps = {
  meta: PageMeta;
  onNavigate: (path: string) => void;
};

export const BreadcrumbSection: FunctionalComponent<BreadcrumbSectionProps> = ({
  meta,
  onNavigate,
}) => {
  const t = useTranslation();

  if (!meta.showHeader) {
    return null;
  }

  return (
    <section class="page-header">
      <nav class="breadcrumb" aria-label={t("breadcrumbs.label")}>
        <ol>
          {meta.breadcrumbs.map((crumb, index) => {
            const isLast = index === meta.breadcrumbs.length - 1;
            if (isLast || !crumb.path) {
              return (
                <li key={`${crumb.labelKey}-${index}`} aria-current="page">
                  <span>{t(crumb.labelKey)}</span>
                </li>
              );
            }
            return (
              <li key={`${crumb.labelKey}-${index}`}>
                <button
                  type="button"
                  class="breadcrumb-link"
                  onClick={() => onNavigate(crumb.path!)}
                >
                  {t(crumb.labelKey)}
                </button>
                <span class="breadcrumb-separator" aria-hidden="true">
                  /
                </span>
              </li>
            );
          })}
        </ol>
      </nav>
      <div class="page-header-content">
        <div>
          <h2 class="text-lg font-semibold text-slate-900">{t(meta.titleKey)}</h2>
          <p class="muted text-sm">{t(meta.descriptionKey)}</p>
        </div>
      </div>
    </section>
  );
};
