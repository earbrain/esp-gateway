import { useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";

type WifiPageProps = {
  path?: string;
};

type WifiFormValues = {
  ssid: string;
  passphrase: string;
};

const isHexKey = (value: string) => /^[0-9a-fA-F]+$/.test(value);

export const WifiPage: FunctionalComponent<WifiPageProps> = () => {
  const [values, setValues] = useState<WifiFormValues>({ ssid: "", passphrase: "" });
  const [submitting, setSubmitting] = useState(false);
  const [submitError, setSubmitError] = useState<string | null>(null);
  const [submitSuccess, setSubmitSuccess] = useState<string | null>(null);

  const handleSsidChange = (event: Event) => {
    const target = event.currentTarget as HTMLInputElement | null;
    if (target) {
      setValues((prev) => ({ ...prev, ssid: target.value }));
    }
  };

  const handlePassphraseChange = (event: Event) => {
    const target = event.currentTarget as HTMLInputElement | null;
    if (target) {
      setValues((prev) => ({ ...prev, passphrase: target.value }));
    }
  };

  const handleSubmit = async (event: Event) => {
    event.preventDefault();
    if (submitting) {
      return;
    }

    const trimmedSsid = values.ssid.trim();
    const passphrase = values.passphrase;

    setSubmitError(null);
    setSubmitSuccess(null);

    const isLengthValid = passphrase.length >= 8 && passphrase.length <= 63;
    const isHexCandidate = passphrase.length === 64 && isHexKey(passphrase);

    if (!trimmedSsid) {
      setSubmitError("Please enter an SSID.");
      return;
    }

    if (!isLengthValid && !isHexCandidate) {
      setSubmitError("Password must be 8-63 characters or a 64-digit hex string.");
      return;
    }

    setSubmitting(true);
    try {
      const response = await fetch("/api/v1/wifi/credentials", {
        method: "POST",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          ssid: trimmedSsid,
          passphrase,
        }),
      });

      if (!response.ok) {
        const json = await response.json().catch(() => null);
        if (json && typeof json.message === "string") {
          throw new Error(json.message);
        }
        throw new Error(`request failed: ${response.status}`);
      }

      const json: { restart_required?: boolean } = await response.json();
      if (json.restart_required) {
        setSubmitSuccess("Saved credentials. A restart is required.");
      } else {
        setSubmitSuccess("Saved credentials.");
      }

      setValues({ ssid: trimmedSsid, passphrase: "" });
    } catch (err) {
      console.error(err);
      setSubmitError(
        err instanceof Error ? err.message : "Failed to save credentials.",
      );
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <section class="space-y-6">
      <div class="card">
        <h2 class="section-title">Update Credentials</h2>
        <form class="grid gap-4" onSubmit={handleSubmit}>
          <label class="form-field">
            <span>SSID</span>
            <input
              type="text"
              value={values.ssid}
              onInput={handleSsidChange}
              placeholder="e.g. gateway-ap"
              required
              class="input"
            />
          </label>
          <label class="form-field">
            <span>Password</span>
            <input
              type="password"
              value={values.passphrase}
              onInput={handlePassphraseChange}
              placeholder="8-63 chars or 64-digit hex"
              minLength={8}
              maxLength={64}
              required
              class="input"
            />
          </label>
          <div class="flex items-center justify-end">
            <button type="submit" disabled={submitting} class="btn-primary">
              {submitting ? "Saving..." : "Save"}
            </button>
          </div>
        </form>
        {submitError && <p class="error-text">{submitError}</p>}
        {submitSuccess && <p class="success-text">{submitSuccess}</p>}
      </div>
    </section>
  );
};
