import { createContext } from "preact";
import type { ComponentChildren } from "preact";
import { useCallback, useContext, useMemo, useState } from "preact/hooks";
import { fallbackLocale, translations, type Locale, type TranslationKey } from "./translations";

type TranslationParams = Record<string, string | number>;

type TranslationContextValue = {
  locale: Locale;
  availableLocales: readonly Locale[];
  t: (key: TranslationKey, params?: TranslationParams) => string;
  setLocale: (next: Locale) => void;
};

const STORAGE_KEY = "esp-gateway.locale";
const supportedLocales: readonly Locale[] = ["en", "ja"];

const formatTemplate = (template: string, params?: TranslationParams): string => {
  if (!params) {
    return template;
  }
  return template.replace(/\{\{(\w+)\}\}/g, (_, token: string) => {
    const value = params[token];
    return value !== undefined && value !== null ? String(value) : "";
  });
};

const TranslationContext = createContext<TranslationContextValue>({
  locale: fallbackLocale,
  availableLocales: supportedLocales,
  t: (key, params) => formatTemplate(translations[fallbackLocale][key] ?? key, params),
  setLocale: () => undefined,
});

const isLocale = (value: string | null): value is Locale =>
  Boolean(value) && supportedLocales.includes(value as Locale);

const resolveInitialLocale = (): Locale => {
  if (typeof window === "undefined") {
    return fallbackLocale;
  }

  const stored = window.localStorage.getItem(STORAGE_KEY);
  if (isLocale(stored)) {
    return stored;
  }

  const navigatorLanguage = window.navigator.language ?? window.navigator.languages?.[0];
  if (navigatorLanguage) {
    const normalized = navigatorLanguage.toLowerCase();
    if (normalized.startsWith("ja")) {
      return "ja";
    }
  }

  return fallbackLocale;
};

export function TranslationProvider(props: { children: ComponentChildren }) {
  const [locale, setLocaleState] = useState<Locale>(() => resolveInitialLocale());

  const setLocale = useCallback((next: Locale) => {
    setLocaleState(next);
    if (typeof window !== "undefined") {
      window.localStorage.setItem(STORAGE_KEY, next);
    }
  }, []);

  const translate = useCallback(
    (key: TranslationKey, params?: TranslationParams) => {
      const template = translations[locale]?.[key] ?? translations[fallbackLocale][key] ?? key;
      return formatTemplate(template, params);
    },
    [locale],
  );

  const value = useMemo<TranslationContextValue>(
    () => ({
      locale,
      availableLocales: supportedLocales,
      t: translate,
      setLocale,
    }),
    [locale, translate, setLocale],
  );

  return <TranslationContext.Provider value={value}>{props.children}</TranslationContext.Provider>;
}

export const useI18n = (): TranslationContextValue => useContext(TranslationContext);

export const useTranslation = () => {
  const { t } = useI18n();
  return t;
};
