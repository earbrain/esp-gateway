import { useState } from "preact/hooks";
import type { FunctionalComponent } from "preact";
import { useApi } from "../hooks/useApi";

type WifiPageProps = {
  path?: string;
};

type WifiFormValues = {
  ssid: string;
  passphrase: string;
};

type WifiResponse = {
  restart_required?: boolean;
};

const isHexKey = (value: string) => /^[0-9a-fA-F]+$/.test(value);

export const WifiPage: FunctionalComponent<WifiPageProps> = () => {
  const [values, setValues] = useState<WifiFormValues>({
    ssid: "",
    passphrase: "",
  });
  const [validationError, setValidationError] = useState<string | null>(null);
  const [success, setSuccess] = useState<string | null>(null);

  const { execute, loading, error, reset } = useApi<WifiResponse>(
    "/api/v1/wifi/credentials",
    {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
    },
  );

  const handleInputChange = (field: keyof WifiFormValues) => (event: Event) => {
    const target = event.currentTarget as HTMLInputElement | null;
    if (target) {
      setValues((prev) => ({ ...prev, [field]: target.value }));
    }
  };

  const handleSubmit = async (event: Event) => {
    event.preventDefault();

    const trimmedSsid = values.ssid.trim();
    const passphrase = values.passphrase;

    const isLengthValid = passphrase.length >= 8 && passphrase.length <= 63;
    const isHexCandidate = passphrase.length === 64 && isHexKey(passphrase);

    if (!trimmedSsid) {
      setValidationError("Please enter an SSID.");
      setSuccess(null);
      reset();
      return;
    }

    if (!isLengthValid && !isHexCandidate) {
      setValidationError(
        "Password must be 8-63 characters or a 64-digit hex string.",
      );
      setSuccess(null);
      reset();
      return;
    }

    setValidationError(null);
    setSuccess(null);

    try {
      const response = await execute({
        body: JSON.stringify({
          ssid: trimmedSsid,
          passphrase,
        }),
      });

      if (response) {
        setSuccess(
          response.restart_required
            ? "Saved credentials. A restart is required."
            : "Saved credentials.",
        );
        setValues({ ssid: trimmedSsid, passphrase: "" });
      }
    } catch (err) {
      console.error(err);
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
              onInput={handleInputChange("ssid")}
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
              onInput={handleInputChange("passphrase")}
              placeholder="8-63 chars or 64-digit hex"
              minLength={8}
              maxLength={64}
              required
              class="input"
            />
          </label>
          <div class="flex items-center justify-end">
            <button type="submit" disabled={loading} class="btn-primary">
              {loading ? "Saving..." : "Save"}
            </button>
          </div>
        </form>
        {(validationError || error) && (
          <p class="error-text">{validationError ?? error}</p>
        )}
        {success && <p class="success-text">{success}</p>}
      </div>
    </section>
  );
};
