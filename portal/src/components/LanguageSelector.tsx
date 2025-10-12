import type { FunctionalComponent } from "preact";
import { useCallback, useEffect, useRef, useState } from "preact/hooks";
import { useI18n, useTranslation } from "../i18n/context";
import type { Locale, TranslationKey } from "../i18n/translations";

const optionKeys: Record<Locale, TranslationKey> = {
  en: "language.option.en",
  ja: "language.option.ja",
};

export const LanguageSelector: FunctionalComponent = () => {
  const { locale, setLocale, availableLocales } = useI18n();
  const t = useTranslation();
  const [open, setOpen] = useState(false);
  const menuRef = useRef<HTMLDivElement | null>(null);

  const handleSelect = useCallback(
    (next: Locale) => {
      if (next !== locale) {
        setLocale(next);
      }
      setOpen(false);
    },
    [locale, setLocale],
  );

  useEffect(() => {
    if (!open) {
      return;
    }
    const handleClick = (event: MouseEvent) => {
      if (
        menuRef.current &&
        event.target instanceof Node &&
        !menuRef.current.contains(event.target)
      ) {
        setOpen(false);
      }
    };
    const handleKeyDown = (event: KeyboardEvent) => {
      if (event.key === "Escape") {
        setOpen(false);
      }
    };
    document.addEventListener("click", handleClick);
    document.addEventListener("keydown", handleKeyDown);
    return () => {
      document.removeEventListener("click", handleClick);
      document.removeEventListener("keydown", handleKeyDown);
    };
  }, [open]);

  return (
    <div class="relative" ref={menuRef}>
      <button
        type="button"
        class="inline-flex items-center gap-2 rounded-lg border border-slate-300 bg-white/90 px-3 py-1.5 text-sm font-medium text-slate-700 shadow-sm backdrop-blur transition hover:bg-slate-100 focus:border-sky-500 focus:outline-none focus:ring-2 focus:ring-sky-200 supports-[backdrop-filter]:bg-white/70"
        aria-haspopup="listbox"
        aria-expanded={open}
        aria-label={t("language.selector.label")}
        onClick={(event) => {
          event.stopPropagation();
          setOpen((prev) => !prev);
        }}
      >
        <span class="font-medium text-slate-800">{t(optionKeys[locale])}</span>
        <svg
          class={`h-4 w-4 text-slate-500 transition-transform ${open ? "rotate-180" : ""}`}
          viewBox="0 0 20 20"
          fill="none"
          stroke="currentColor"
          stroke-width="1.5"
          aria-hidden="true"
        >
          <path d="M6 8l4 4 4-4" stroke-linecap="round" stroke-linejoin="round" />
        </svg>
      </button>
      {open && (
        <div class="absolute right-0 z-30 mt-2 w-44 overflow-hidden rounded-lg border border-slate-200 bg-white shadow-xl ring-1 ring-slate-200/70">
          <ul role="listbox" aria-label={t("language.selector.label")} class="py-1">
            {availableLocales.map((code) => {
              const isActive = code === locale;
              return (
                <li key={code}>
                  <button
                    type="button"
                    role="option"
                    aria-selected={isActive}
                    class={`flex w-full items-center px-3 py-2 text-left text-sm transition ${
                      isActive
                        ? "bg-sky-50 text-sky-700 font-medium"
                        : "text-slate-600 hover:bg-slate-100"
                    }`}
                    onClick={() => handleSelect(code)}
                  >
                    <span>{t(optionKeys[code])}</span>
                  </button>
                </li>
              );
            })}
          </ul>
        </div>
      )}
    </div>
  );
};
