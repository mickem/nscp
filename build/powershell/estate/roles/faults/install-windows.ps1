# "Acme Agent" for the faults estate role, on Windows: a stand-in vendor
# application whose parts misbehave on demand, so a fault the role's bundle
# scripts switch on has a name in the host's facts (software.installed,
# services.installed) and in its process list. Runs as SYSTEM before
# NSClient++ is installed (see role.json and README.md, "Adding a role").
#
#   C:\Program Files\Acme\acme-indexer.exe  `cpu` spins one thread per core;
#                                           `memory <bytes>` allocates and
#                                           touches that much and holds it
#   C:\Program Files\Acme\acme-sync.exe     the acme-sync service, which
#                                           crashes while
#                                           C:\ProgramData\Acme\sync.fail
#                                           exists; its recovery action
#                                           restarts it, so it crash-loops
#                                           and heals itself once the file
#                                           is gone
#   C:\ProgramData\Acme\cache               where the disk filler puts its file
#
# Both programs are compiled here from the C# below (Add-Type), so the image
# needs nothing but Windows PowerShell 5.1 and .NET Framework 4.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

function Write-Step([string]$Text) { Write-Output "== $Text" }

$root = Join-Path $env:ProgramFiles 'Acme'
$data = Join-Path $env:ProgramData 'Acme'
New-Item -ItemType Directory -Force -Path $root, (Join-Path $data 'cache') | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $data 'sync.fail')

Write-Step "Compiling acme-indexer.exe"
$indexer = @'
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;

public static class AcmeIndexer {
    // A safety net: a test that dies half way must not leave the VM hogged.
    static readonly TimeSpan Lifetime = TimeSpan.FromHours(1);

    public static int Main(string[] args) {
        string mode = args.Length > 0 ? args[0] : "";
        if (mode == "cpu") {
            // Below normal, so the agent that is meant to report it stays
            // responsive; the cores are still saturated.
            Process.GetCurrentProcess().PriorityClass = ProcessPriorityClass.BelowNormal;
            for (int i = 0; i < Environment.ProcessorCount; i++) {
                Thread t = new Thread(() => { long n = 0; while (true) { n++; } });
                t.IsBackground = true;
                t.Start();
            }
            Thread.Sleep(Lifetime);
            return 0;
        }
        if (mode == "memory" && args.Length > 1) {
            long bytes = long.Parse(args[1]);
            const int chunk = 64 << 20;
            List<byte[]> held = new List<byte[]>();
            for (long done = 0; done < bytes; done += chunk) {
                byte[] block = new byte[(int)Math.Min(chunk, bytes - done)];
                // Touch every page, so it is resident and not just committed.
                for (int i = 0; i < block.Length; i += 4096) block[i] = 1;
                held.Add(block);
            }
            Thread.Sleep(Lifetime);
            GC.KeepAlive(held);
            return 0;
        }
        Console.Error.WriteLine("usage: acme-indexer cpu | memory <bytes>");
        return 2;
    }
}
'@
Add-Type -TypeDefinition $indexer -OutputType ConsoleApplication -OutputAssembly (Join-Path $root 'acme-indexer.exe')

Write-Step "Compiling acme-sync.exe"
$sync = @'
using System;
using System.IO;
using System.ServiceProcess;
using System.Threading;

public class AcmeSync : ServiceBase {
    static readonly string Trigger = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.CommonApplicationData), @"Acme\sync.fail");

    public AcmeSync() { ServiceName = "acme-sync"; }

    protected override void OnStart(string[] args) {
        Thread worker = new Thread(Run);
        worker.IsBackground = true;
        worker.Start();
    }

    static void Run() {
        while (true) {
            if (File.Exists(Trigger)) {
                // Exiting without reporting SERVICE_STOPPED is a crash to the
                // service control manager, which runs the recovery action.
                Console.Error.WriteLine("acme-sync: cannot open catalog.db: database disk image is malformed");
                Environment.Exit(1);
            }
            Thread.Sleep(2000);
        }
    }

    public static void Main() { ServiceBase.Run(new AcmeSync()); }
}
'@
Add-Type -TypeDefinition $sync -ReferencedAssemblies System.ServiceProcess -OutputType ConsoleApplication `
    -OutputAssembly (Join-Path $root 'acme-sync.exe')

Write-Step "Installing the acme-sync service"
$exe = Join-Path $root 'acme-sync.exe'
if (-not (Get-Service -Name 'acme-sync' -ErrorAction SilentlyContinue)) {
    New-Service -Name 'acme-sync' -DisplayName 'Acme catalogue sync' -BinaryPathName "`"$exe`"" -StartupType Automatic | Out-Null
}
# Restart 10 s after every crash, forever: the last action repeats.
& sc.exe failure acme-sync reset= 86400 actions= restart/10000 | Out-Null
if ($LASTEXITCODE -ne 0) { throw "sc.exe failure acme-sync exited $LASTEXITCODE" }
Start-Service -Name 'acme-sync'

Write-Step "Registering Acme Agent as installed software"
$uninstall = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\AcmeAgent'
New-Item -Force -Path $uninstall | Out-Null
Set-ItemProperty -Path $uninstall -Name DisplayName -Value 'Acme Agent'
Set-ItemProperty -Path $uninstall -Name DisplayVersion -Value '2.3.1'
Set-ItemProperty -Path $uninstall -Name Publisher -Value 'Acme Corp'
Set-ItemProperty -Path $uninstall -Name InstallLocation -Value $root

(Get-Service -Name 'acme-sync').Status
Write-Output "acme-agent installed"
