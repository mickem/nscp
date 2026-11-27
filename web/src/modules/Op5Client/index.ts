import { HOSTNAME_WIDGET } from "../kit/hostnameWidget.ts";
import type { ModulePage } from "../types.ts";

const Op5Client: ModulePage = {
  id: "Op5Client",
  sections: { "/settings/op5": HOSTNAME_WIDGET },
};

export default Op5Client;
