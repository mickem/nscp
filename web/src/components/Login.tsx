import Stack from "@mui/material/Stack";
import { Box, Card, CardActions, CardContent, InputAdornment, TextField, Toolbar } from "@mui/material";
import Grid from "@mui/material/Grid";
import AppBar from "@mui/material/AppBar";
import Button from "@mui/material/Button";
import { AccountCircle } from "@mui/icons-material";
import PasswordIcon from "@mui/icons-material/Password";
import { useAuthentication } from "../common/hooks/auth.ts";
import { useEffect, useState } from "react";
import NscpAlert from "./atoms/NscpAlert";
import Typography from "@mui/material/Typography";

export default function Login() {
  const { login, restoreToken } = useAuthentication();
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [error, setError] = useState<string | undefined>(undefined);

  const doLogin = async () => {
    try {
      await login(username, password);
    } catch (error) {
      console.error("Login failed:", error);
      setError("Login failed. Please check your credentials and try again.");
    }
  };
  useEffect(() => {
    restoreToken();
  }, [restoreToken]);
  return (
    <>
      <Box sx={{ width: "100vw", height: "100vh" }}>
        <AppBar position="static">
          <Toolbar>
            <Typography variant="h6">NSClient++</Typography>
          </Toolbar>
        </AppBar>

        <Box sx={{ p: 3 }}>
          <Toolbar />
          <Grid container sx={{ justifyContent: "center" }}>
            <Grid>
              <Card sx={{ maxWidth: 400, p: 3 }}>
                <CardContent>
                  <Stack direction="column" spacing={3}>
                    {error && (
                      <NscpAlert severity="error" text={error} />
                    )}
                    <TextField
                      label="Username"
                      value={username}
                      onChange={(e) => setUsername(e.target.value)}
                      slotProps={{
                        input: {
                          startAdornment: (
                            <InputAdornment position="start">
                              <AccountCircle />
                            </InputAdornment>
                          ),
                        },
                      }}
                      variant="standard"
                    />
                    <TextField
                      label="Password"
                      value={password}
                      onChange={(e) => setPassword(e.target.value)}
                      type="password"
                      slotProps={{
                        input: {
                          startAdornment: (
                            <InputAdornment position="start">
                              <PasswordIcon />
                            </InputAdornment>
                          ),
                        },
                      }}
                      variant="standard"
                    />
                  </Stack>
                </CardContent>
                <CardActions>
                  <Button size="small" onClick={doLogin}>
                    Login
                  </Button>
                </CardActions>
              </Card>
            </Grid>
          </Grid>
        </Box>
      </Box>
    </>
  );
}
