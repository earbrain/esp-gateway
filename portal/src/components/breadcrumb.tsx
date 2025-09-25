import type { FunctionalComponent } from "preact";

export type Breadcrumb = {
  label: string;
  path?: string;
};

export type PageMeta = {
  showHeader: boolean;
  title: string;
  description: string;
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
  if (!meta.showHeader) {
    return null;
  }

  return (
    <section class="page-header">
      <nav class="breadcrumb" aria-label="Breadcrumb">
        <ol>
          {meta.breadcrumbs.map((crumb, index) => {
            const isLast = index === meta.breadcrumbs.length - 1;
            if (isLast || !crumb.path) {
              return (
                <li key={`${crumb.label}-${index}`} aria-current="page">
                  <span>{crumb.label}</span>
                </li>
              );
            }
            return (
              <li key={`${crumb.label}-${index}`}>
                <button
                  type="button"
                  class="breadcrumb-link"
                  onClick={() => onNavigate(crumb.path!)}
                >
                  {crumb.label}
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
          <h2 class="text-lg font-semibold text-slate-900">{meta.title}</h2>
          <p class="muted text-sm">{meta.description}</p>
        </div>
      </div>
    </section>
  );
};
