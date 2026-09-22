using System.Diagnostics;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Security.Cryptography.X509Certificates;
using System.Text.Json;

namespace Firawynix.Kdenlive.Launcher;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args)
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new LauncherForm(args));
    }
}

internal sealed class LauncherForm : Form
{
    private const string UpdateFeed = "https://jogos.firawynix.com.br/api/games/kdenlive/windows/atualizacao.json";
    private const string PackagePath = "/api/games/kdenlive/windows/x64/arquivo";
    private const string SignerThumbprint = "9A2EFF2483185C9A900F2797D7A9CBD5E8A12893";

    [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
    private static extern int GetCurrentPackageFullName(ref int packageFullNameLength, nint packageFullName);
    private readonly string[] _arguments;
    private readonly CancellationTokenSource _cancellation = new();
    private readonly Label _status = new();
    private readonly ProgressBar _progress = new();
    private readonly Button _openNow = new();
    private bool _launched;

    public LauncherForm(string[] arguments)
    {
        _arguments = arguments.Where(argument => !argument.Equals("--no-update", StringComparison.OrdinalIgnoreCase)).ToArray();
        Text = "Firawynix - Kdenlive";
        ClientSize = new Size(540, 184);
        MinimumSize = new Size(540, 223);
        MaximizeBox = false;
        StartPosition = FormStartPosition.CenterScreen;
        BackColor = Color.FromArgb(10, 14, 24);
        ForeColor = Color.White;
        Font = new Font("Segoe UI", 10F);

        var title = new Label {
            AutoSize = true,
            Text = "Firawynix - Kdenlive",
            Font = new Font("Segoe UI Semibold", 18F),
            ForeColor = Color.FromArgb(34, 211, 238),
            Location = new Point(24, 20),
        };
        _status.AutoSize = false;
        _status.Text = "Verificando atualizações seguras…";
        _status.Location = new Point(27, 67);
        _status.Size = new Size(486, 24);
        _progress.Location = new Point(28, 101);
        _progress.Size = new Size(484, 13);
        _progress.Style = ProgressBarStyle.Marquee;
        _progress.MarqueeAnimationSpeed = 28;
        _openNow.Text = "Abrir agora";
        _openNow.Size = new Size(124, 34);
        _openNow.Location = new Point(388, 132);
        _openNow.FlatStyle = FlatStyle.Flat;
        _openNow.FlatAppearance.BorderColor = Color.FromArgb(34, 211, 238);
        _openNow.ForeColor = Color.FromArgb(34, 211, 238);
        _openNow.Click += (_, _) => {
            _cancellation.Cancel();
            LaunchEditor();
        };

        Controls.AddRange([title, _status, _progress, _openNow]);
        Shown += async (_, _) => await CheckAndLaunchAsync();
        FormClosed += (_, _) => _cancellation.Cancel();
    }

    private async Task CheckAndLaunchAsync()
    {
        if (Environment.GetCommandLineArgs().Any(argument => argument.Equals("--no-update", StringComparison.OrdinalIgnoreCase)) || IsStorePackage()) {
            LaunchEditor();
            return;
        }

        try {
            using var client = new HttpClient { Timeout = Timeout.InfiniteTimeSpan };
            client.DefaultRequestHeaders.UserAgent.ParseAdd("Firawynix-Kdenlive-Launcher/1.0");
            using var feedTimeout = CancellationTokenSource.CreateLinkedTokenSource(_cancellation.Token);
            feedTimeout.CancelAfter(TimeSpan.FromSeconds(20));
            using var response = await client.GetAsync(UpdateFeed, feedTimeout.Token);
            response.EnsureSuccessStatusCode();
            using var feed = JsonDocument.Parse(await response.Content.ReadAsStreamAsync(feedTimeout.Token));
            var root = feed.RootElement;
            string tag = root.GetProperty("version").GetString() ?? string.Empty;
            string current = Assembly.GetExecutingAssembly().GetName().Version?.ToString() ?? "0";
            if (CompareVersions(tag, current) <= 0) {
                SetStatus("Você já está na versão mais recente.");
                await Task.Delay(450, _cancellation.Token);
                LaunchEditor();
                return;
            }

            var package = root.GetProperty("architectures").GetProperty("x64");
            string url = package.GetProperty("url").GetString() ?? string.Empty;
            string expectedHash = package.GetProperty("sha256").GetString() ?? string.Empty;
            long expectedSize = package.GetProperty("size").GetInt64();
            if (!Uri.TryCreate(url, UriKind.Absolute, out var packageUri) ||
                packageUri.Scheme != Uri.UriSchemeHttps ||
                packageUri.Host != "jogos.firawynix.com.br" ||
                packageUri.AbsolutePath != PackagePath ||
                expectedHash.Length != 64 ||
                !expectedHash.All(Uri.IsHexDigit) ||
                expectedSize <= 0) {
                throw new InvalidDataException("O manifesto da atualização é inválido.");
            }

            DialogResult choice = MessageBox.Show(this,
                $"A versão {tag} está disponível. Deseja baixar e instalar agora?\n\nO projeto aberto não será incluído no download.",
                "Atualização disponível", MessageBoxButtons.YesNo, MessageBoxIcon.Information);
            if (choice != DialogResult.Yes) {
                LaunchEditor();
                return;
            }

            string updateFolder = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "Firawynix-Kdenlive", "updates");
            Directory.CreateDirectory(updateFolder);
            string installerPath = Path.Combine(updateFolder, "Firawynix-Kdenlive-Setup-x64.exe");

            SetStatus($"Baixando {tag}…");
            await DownloadAsync(client, url, installerPath, expectedSize, _cancellation.Token);
            SetStatus("Verificando a integridade da atualização…");
            if (new FileInfo(installerPath).Length != expectedSize) {
                File.Delete(installerPath);
                throw new InvalidDataException("O tamanho da atualização não corresponde ao manifesto.");
            }
            string actualHash;
            await using (var stream = File.OpenRead(installerPath)) {
                actualHash = Convert.ToHexString(await SHA256.HashDataAsync(stream, _cancellation.Token));
            }
            if (!actualHash.Equals(expectedHash, StringComparison.OrdinalIgnoreCase)) {
                File.Delete(installerPath);
                throw new InvalidDataException("A atualização baixada não passou na verificação de integridade.");
            }
            using var signer = new X509Certificate2(X509Certificate.CreateFromSignedFile(installerPath));
            if (!signer.Thumbprint.Equals(SignerThumbprint, StringComparison.OrdinalIgnoreCase)) {
                File.Delete(installerPath);
                throw new InvalidDataException("A atualização não foi assinada pela Firawynix.");
            }

            Process.Start(new ProcessStartInfo(installerPath, "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-") { UseShellExecute = true });
            _launched = true;
            Close();
        } catch (OperationCanceledException) {
            if (!_launched) {
                LaunchEditor();
            }
        } catch (Exception error) {
            SetStatus("Não foi possível verificar atualizações. Abrindo a versão instalada.");
            Debug.WriteLine(error);
            await Task.Delay(900);
            LaunchEditor();
        }
    }

    private async Task DownloadAsync(HttpClient client, string url, string destination, long expectedSize, CancellationToken cancellationToken)
    {
        using var response = await client.GetAsync(url, HttpCompletionOption.ResponseHeadersRead, cancellationToken);
        response.EnsureSuccessStatusCode();
        long total = response.Content.Headers.ContentLength ?? expectedSize;
        if (total != expectedSize) {
            throw new InvalidDataException("O tamanho anunciado para a atualização não corresponde ao manifesto.");
        }
        await using var source = await response.Content.ReadAsStreamAsync(cancellationToken);
        await using var output = new FileStream(destination, FileMode.Create, FileAccess.Write, FileShare.None, 1024 * 128, true);
        byte[] buffer = new byte[1024 * 128];
        long received = 0;
        int read;
        while ((read = await source.ReadAsync(buffer, cancellationToken)) > 0) {
            await output.WriteAsync(buffer.AsMemory(0, read), cancellationToken);
            received += read;
            if (received > expectedSize) {
                throw new InvalidDataException("A atualização excedeu o tamanho esperado.");
            }
            if (total > 0) {
                int percent = (int)Math.Clamp(received * 100 / total, 0, 100);
                BeginInvoke(() => {
                    _progress.Style = ProgressBarStyle.Continuous;
                    _progress.Value = percent;
                    _status.Text = $"Baixando atualização… {percent}%";
                });
            }
        }
    }

    private void LaunchEditor()
    {
        if (_launched) {
            return;
        }
        _launched = true;
        string editor = Path.Combine(AppContext.BaseDirectory, "bin", "firawynix-kdenlive.exe");
        if (!File.Exists(editor)) {
            MessageBox.Show(this, $"O editor não foi encontrado em:\n{editor}", "Firawynix - Kdenlive", MessageBoxButtons.OK, MessageBoxIcon.Error);
            Close();
            return;
        }
        var start = new ProcessStartInfo(editor) { UseShellExecute = true, WorkingDirectory = Path.GetDirectoryName(editor)! };
        foreach (string argument in _arguments) {
            start.ArgumentList.Add(argument);
        }
        Process.Start(start);
        Close();
    }

    private void SetStatus(string text)
    {
        if (IsDisposed) {
            return;
        }
        BeginInvoke(() => _status.Text = text);
    }

    private static int CompareVersions(string left, string right)
    {
        int[] a = ExtractVersion(left);
        int[] b = ExtractVersion(right);
        for (int index = 0; index < Math.Max(a.Length, b.Length); ++index) {
            int av = index < a.Length ? a[index] : 0;
            int bv = index < b.Length ? b[index] : 0;
            if (av != bv) {
                return av.CompareTo(bv);
            }
        }
        return 0;
    }

    private static int[] ExtractVersion(string value) =>
        System.Text.RegularExpressions.Regex.Matches(value, "\\d+").Select(match => int.Parse(match.Value)).ToArray();

    private static bool IsStorePackage()
    {
        int length = 0;
        return OperatingSystem.IsWindows() && GetCurrentPackageFullName(ref length, 0) == 122;
    }
}
