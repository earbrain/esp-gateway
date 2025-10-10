import type { FunctionalComponent } from "preact";

type WifiSignalIndicatorProps = {
  strength: number;
  label?: string;
};

const getSignalLevel = (strength: number) => {
  if (strength >= 80) return 4;
  if (strength >= 60) return 3;
  if (strength >= 40) return 2;
  if (strength >= 20) return 1;
  return 0;
};

export const WifiSignalIndicator: FunctionalComponent<WifiSignalIndicatorProps> = ({
  strength,
  label,
}) => {
  const level = getSignalLevel(strength);
  const bars = [0, 1, 2, 3];
  const heights = [8, 12, 16, 20];

  return (
    <span class="flex items-end gap-1" aria-label={label ?? `Signal strength ${level} of 4`}>
      {bars.map((index) => (
        <span
          key={index}
          class={`w-1.5 rounded-full transition ${
            index < level ? "bg-slate-900" : "bg-slate-300"
          }`}
          style={{ height: `${heights[index]}px` }}
        ></span>
      ))}
    </span>
  );
};
