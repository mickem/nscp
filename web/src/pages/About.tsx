import { Box, Chip, Divider, Link, Stack, Table, TableBody, TableCell, TableContainer, TableHead, TableRow, Typography } from "@mui/material";
import Paper from "@mui/material/Paper";
import licenseData from "../generated/licenses.json";

// Generated at build time from the production dependency tree; see
// scripts/generate-licenses.mjs.
const packages = licenseData.packages;

// Count of each distinct license, most common first, for the summary chips.
const licenseSummary = Object.entries(
  packages.reduce<Record<string, number>>((acc, pkg) => {
    acc[pkg.license] = (acc[pkg.license] || 0) + 1;
    return acc;
  }, {}),
).sort((a, b) => b[1] - a[1]);

export default function About() {
  return (
    <Box sx={{ maxWidth: 900 }}>
      <Stack direction="column" spacing={3} sx={{ mt: 2 }}>
        <Box>
          <Typography variant="h4" gutterBottom>
            NSClient++
          </Typography>
          <Typography variant="body1">
            A monitoring agent and management interface. See{" "}
            <Link href="https://nsclient.org" target="_blank" rel="noopener noreferrer">
              nsclient.org
            </Link>{" "}
            and the{" "}
            <Link href="https://github.com/mickem/nscp" target="_blank" rel="noopener noreferrer">
              source repository
            </Link>
            .
          </Typography>
          <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
            Licensed under Apache-2.0 OR GPL-2.0-only.
          </Typography>
        </Box>

        <Divider />

        <Box>
          <Typography variant="h6" gutterBottom>
            Third-party components
          </Typography>
          <Typography variant="body2" color="text.secondary" sx={{ mb: 2 }}>
            This web UI bundles {packages.length} open-source components, listed below for attribution. They remain under
            their respective licenses.
          </Typography>

          <Stack direction="row" spacing={1} useFlexGap sx={{ flexWrap: "wrap", mb: 2 }}>
            {licenseSummary.map(([license, count]) => (
              <Chip key={license} label={`${license} (${count})`} size="small" variant="outlined" />
            ))}
          </Stack>

          <TableContainer component={Paper} variant="outlined" sx={{ maxHeight: 480 }}>
            <Table size="small" stickyHeader aria-label="third-party web components">
              <TableHead>
                <TableRow>
                  <TableCell>Component</TableCell>
                  <TableCell>Version</TableCell>
                  <TableCell>License</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {packages.map((dep) => (
                  <TableRow key={dep.name} hover>
                    <TableCell>
                      <Link href={dep.url} target="_blank" rel="noopener noreferrer">
                        {dep.name}
                      </Link>
                    </TableCell>
                    <TableCell>{dep.version}</TableCell>
                    <TableCell>{dep.license}</TableCell>
                  </TableRow>
                ))}
              </TableBody>
            </Table>
          </TableContainer>
        </Box>
      </Stack>
    </Box>
  );
}
