import { render } from "preact";
import { App } from "./app";
import { TranslationProvider } from "./i18n/context";
import "./styles.css";

async function enableMocking() {
  if (import.meta.env.DEV) {
    const { worker } = await import('./mocks/browser');

    return worker.start({
      onUnhandledRequest: 'bypass',
    });
  }
}

enableMocking().then(() => {
  render(
    <TranslationProvider>
      <App />
    </TranslationProvider>,
    document.getElementById("root")!,
  );
});
