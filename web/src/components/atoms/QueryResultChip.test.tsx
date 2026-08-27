import { describe, expect, it } from "vitest";
import { render, screen } from "@testing-library/react";
import { QueryResultChip } from "./QueryResultChip";

describe("QueryResultChip", () => {
  it.each([
    [0, "Ok", "colorSuccess"],
    [1, "Warning", "colorWarning"],
    [2, "Critical", "colorError"],
    [3, "Unknown", "colorSecondary"],
  ] as const)("renders result %i as %s", (result, label, colorClass) => {
    render(<QueryResultChip result={result} />);
    const chip = screen.getByText(label);
    expect(chip).toBeInTheDocument();
    // The MUI color class lives on the chip root, one level up from the label.
    expect(chip.parentElement?.className).toContain(colorClass);
  });
});
