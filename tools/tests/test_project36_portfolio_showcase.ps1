# Observable-behavior test for the Project 36 portfolio showcase.
#
# This test launches the built executable and inspects the frames it actually
# renders. User-visible requirements are verified through rendered pixels,
# process liveness, or file content. Narrow source-level guards complement those
# frame tests where exact call wiring/default scene units cannot be read from pixels.
[CmdletBinding()]
param(
    [switch]$KeepFrames
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$script:Failures = [System.Collections.Generic.List[string]]::new()

function Assert-True {
    param([bool]$Condition, [string]$Message)

    if (-not $Condition) {
        $script:Failures.Add($Message)
    }
    else {
        Write-Host "  ok   $Message"
    }
}

Add-Type -AssemblyName System.Drawing

if (-not ('ShowcaseWin32' -as [type])) {
    Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public static class ShowcaseWin32 {
  public const uint WM_CLOSE = 0x0010;
  public const uint WM_LBUTTONDOWN = 0x0201;
  public const uint WM_LBUTTONUP = 0x0202;
  public const int MK_LBUTTON = 0x0001;
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out int lpdwProcessId);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);
  [DllImport("user32.dll")] public static extern int GetWindowTextLength(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hWnd);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hWnd, ref POINT lpPoint);
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);
  public struct RECT { public int Left; public int Top; public int Right; public int Bottom; }
  public struct POINT { public int X; public int Y; }
}
"@
}

if (-not ('ShowcaseFrame' -as [type])) {
    # Pure array math, deliberately free of any System.Drawing dependency: the
    # caller hands over the raw 32bpp scan lines, so this compiles the same way
    # on Windows PowerShell and on PowerShell 7 (where System.Drawing lives in
    # System.Drawing.Common and drags in extra reference assemblies).
    Add-Type @"
using System;

// A captured client-area frame reduced to 8-bit luminance so comparisons stay
// fast enough to run inside a PowerShell test.
public class ShowcaseFrame {
  public int Width;
  public int Height;
  public byte[] Pixels;
  public byte[] Red;
  public byte[] Green;
  public byte[] Blue;

  public static ShowcaseFrame FromBgra(byte[] buffer, int stride, int width, int height) {
    ShowcaseFrame frame = new ShowcaseFrame();
    frame.Width = width;
    frame.Height = height;
    frame.Pixels = new byte[width * height];
    frame.Red = new byte[width * height];
    frame.Green = new byte[width * height];
    frame.Blue = new byte[width * height];
    for (int y = 0; y < height; ++y) {
      int rowBase = y * stride;
      int outBase = y * width;
      for (int x = 0; x < width; ++x) {
        int b = buffer[rowBase + x * 4 + 0];
        int g = buffer[rowBase + x * 4 + 1];
        int r = buffer[rowBase + x * 4 + 2];
        int index = outBase + x;
        frame.Pixels[index] = (byte)((r * 77 + g * 151 + b * 28) >> 8);
        frame.Red[index] = (byte)r;
        frame.Green[index] = (byte)g;
        frame.Blue[index] = (byte)b;
      }
    }
    return frame;
  }

  // Every region must lie wholly inside the frame. The earlier version silently
  // shrank an out-of-range rectangle to nothing, which let a "<= tolerance"
  // assertion pass vacuously over a region containing no pixels at all. An
  // impossible region is a defect in the test, so say so loudly instead.
  private static void Require(ShowcaseFrame f, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || x + w > f.Width || y + h > f.Height) {
      throw new ArgumentOutOfRangeException("region", string.Format(
        "region ({0},{1}) {2}x{3} is not wholly inside the {4}x{5} frame", x, y, w, h, f.Width, f.Height));
    }
  }

  // Fraction of pixels in the region whose luminance differs by more than
  // 'tolerance'. 0.0 means the two frames render the region identically.
  public static double DiffRatio(ShowcaseFrame a, ShowcaseFrame b, int x, int y, int w, int h, int tolerance) {
    if (a.Width != b.Width || a.Height != b.Height) { return 1.0; }
    Require(a, x, y, w, h);
    long changed = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        int delta = a.Pixels[rowBase + xx] - b.Pixels[rowBase + xx];
        if (delta < 0) { delta = -delta; }
        if (delta > tolerance) { ++changed; }
      }
    }
    return (double)changed / (double)(w * h);
  }

  // Mean absolute luminance difference over a region, in 0-255 units. DiffRatio
  // above answers "how much of this region moved"; this answers "by how much",
  // which is what separates a cross-fade from a cut. A fraction-of-changed-pixels
  // measure saturates - once a limb has moved past its own width, moving further
  // does not raise it - so a 0.6 s fade and an instant clip switch both read close
  // to "all of the character changed" and become indistinguishable. Mean absolute
  // difference keeps rising with the size of the change, so the step across a set
  // boundary can be compared against an ordinary step of the same duration.
  public static double MeanAbsDiff(ShowcaseFrame a, ShowcaseFrame b, int x, int y, int w, int h) {
    if (a.Width != b.Width || a.Height != b.Height) { return 255.0; }
    Require(a, x, y, w, h);
    long sum = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        int delta = a.Pixels[rowBase + xx] - b.Pixels[rowBase + xx];
        sum += (delta < 0) ? -delta : delta;
      }
    }
    return (double)sum / (double)(w * h);
  }

  // Fraction of pixels in the region at or above 'threshold' luminance. Used to
  // prove that legible light-on-dark HUD text was actually rasterised.
  public static double BrightRatio(ShowcaseFrame a, int x, int y, int w, int h, int threshold) {
    Require(a, x, y, w, h);
    long bright = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        if (a.Pixels[rowBase + xx] >= threshold) { ++bright; }
      }
    }
    return (double)bright / (double)(w * h);
  }

  // Fraction of pixels in the region at or below 'threshold' luminance. The HUD
  // panel is drawn opaque at (0.04, 0.05, 0.08), i.e. luminance ~12, so this reads
  // near 1.0 over the panel and collapses over any lit scene content behind it.
  // Paired with BrightRatio it identifies the HUD rather than merely bright pixels:
  // scene background can be bright OR dark, but it cannot be almost entirely
  // near-black and still carry legible near-white text.
  public static double DarkRatio(ShowcaseFrame a, int x, int y, int w, int h, int threshold) {
    Require(a, x, y, w, h);
    long dark = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        if (a.Pixels[rowBase + xx] <= threshold) { ++dark; }
      }
    }
    return (double)dark / (double)(w * h);
  }

  // Count of distinct luminance byte values (0-255) present in the region. A
  // photographic scene - stone, foliage, and a bright opening all sitting at
  // different brightnesses - rasterises across a wide spread of luminance
  // values, while a flat clear-colour fill rasterises to exactly one value
  // everywhere (or a small handful, if antialiasing at its border lands inside
  // the region). This is variety, not brightness: a uniformly bright or
  // uniformly dark region still collapses to one value, so the count cannot be
  // fooled by a fill that merely happens to be light or dark rather than grey.
  public static int DistinctLuminanceCount(ShowcaseFrame a, int x, int y, int w, int h) {
    Require(a, x, y, w, h);
    bool[] seen = new bool[256];
    int distinct = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        int v = a.Pixels[rowBase + xx];
        if (!seen[v]) { seen[v] = true; ++distinct; }
      }
    }
    return distinct;
  }

  private static bool IsNavyAccent(int r, int g, int b, int luma) {
    return b > r + 35 && b > g + 20 && b > 60 && luma < 95;
  }

  private static bool IsToonPaletteCandidate(int r, int g, int b, int luma) {
    int maxChannel = Math.Max(r, Math.Max(g, b));
    int minChannel = Math.Min(r, Math.Min(g, b));
    int chroma = maxChannel - minChannel;
    return luma >= 70 && luma < 220 && minChannel >= 55 &&
      chroma > 5 && chroma <= 80 && !IsNavyAccent(r, g, b, luma);
  }

  // Unit-sized negative control for the rendered palette probe. This exact dark
  // cool colour satisfies the old costume mask and the navy-shoe predicate; it
  // must now be classified only as the model accent, never as a toon shadow.
  public static bool IsToonPaletteCandidateRgb(int r, int g, int b) {
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
      throw new ArgumentOutOfRangeException("RGB channels must be bytes");
    }
    int luma = (r * 77 + g * 151 + b * 28) >> 8;
    return IsToonPaletteCandidate(r, g, b, luma);
  }

  // SampleModel's shoes carry a saturated navy-blue accent. Counting that
  // rendered accent in each independently moving slot distinguishes the agreed
  // four-copies composition from the old AliceEnemy1 rabbit model, whose boots
  // contain none of these pixels in the same lower-body band.
  public static int BlueAccentCount(ShowcaseFrame a, int x, int y, int w, int h) {
    Require(a, x, y, w, h);
    int count = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        int index = rowBase + xx;
        int r = a.Red[index];
        int g = a.Green[index];
        int b = a.Blue[index];
        int luma = a.Pixels[index];
        if (IsNavyAccent(r, g, b, luma)) { ++count; }
      }
    }
    return count;
  }

  // Palette measurements over pixels belonging to the moving character in both
  // frames. Requiring motion removes the static floor/background; requiring the
  // pale, moderately chromatic costume range removes the saturated navy shoes.
  // The result is { cool shadow, warm light, strong purple, candidate count }.
  public static double[] MovingToonPaletteRatios(ShowcaseFrame a, ShowcaseFrame other,
                                                  int x, int y, int w, int h,
                                                  int motionTolerance) {
    if (a.Width != other.Width || a.Height != other.Height) {
      throw new ArgumentException("toon palette frames must be the same size");
    }
    Require(a, x, y, w, h);
    Require(other, x, y, w, h);
    long candidates = 0;
    long cool = 0;
    long warm = 0;
    long strongPurple = 0;
    for (int yy = y; yy < y + h; ++yy) {
      int rowBase = yy * a.Width;
      for (int xx = x; xx < x + w; ++xx) {
        int index = rowBase + xx;
        int r = a.Red[index];
        int g = a.Green[index];
        int b = a.Blue[index];
        int rOther = other.Red[index];
        int gOther = other.Green[index];
        int bOther = other.Blue[index];
        int maxDelta = Math.Max(Math.Abs(r - rOther),
          Math.Max(Math.Abs(g - gOther), Math.Abs(b - bOther)));
        if (maxDelta <= motionTolerance) { continue; }

        int luma = a.Pixels[index];
        int otherLuma = other.Pixels[index];
        bool currentCandidate = IsToonPaletteCandidate(r, g, b, luma);
        bool otherCandidate = IsToonPaletteCandidate(rOther, gOther, bOther, otherLuma);
        if (!currentCandidate || !otherCandidate) { continue; }

        ++candidates;
        if (b > r + 8 && b > g + 3) { ++cool; }
        if (r > b + 2 && r >= g - 6) { ++warm; }
        if (b > r + 38 && b > g + 18) { ++strongPurple; }
      }
    }
    if (candidates == 0) { return new double[] { 0.0, 0.0, 1.0, 0.0 }; }
    return new double[] {
      (double)cool / (double)candidates,
      (double)warm / (double)candidates,
      (double)strongPurple / (double)candidates,
      (double)candidates
    };
  }

  // A coarse ink profile of a region, rendered as a comparable string: the number
  // of near-white (text) pixels in each 8 px column, quantised. The HUD panel is
  // opaque and fixed-geometry, so one caption rasterises to one profile on every
  // frame that shows it, while a different clip line-up moves enough ink between
  // columns to change it.
  //
  // Quantising is what keeps the comparison honest. A raw pixel hash would differ
  // whenever a single captured byte wobbled, which would satisfy "these two
  // signatures differ" no matter what the clip assignment did - a vacuous pass.
  // Bucketing the per-column ink makes an identical caption produce an identical
  // string, so the companion assertion below (the signature is STABLE inside one
  // set) has real force, and the two together can only both hold if the line-up
  // genuinely changes at the set boundary and only there.
  public static string Signature(ShowcaseFrame a, int x, int y, int w, int h) {
    Require(a, x, y, w, h);
    const int band = 8;
    System.Text.StringBuilder sb = new System.Text.StringBuilder();
    for (int cx = x; cx < x + w; cx += band) {
      int right = Math.Min(cx + band, x + w);
      int ink = 0;
      for (int yy = y; yy < y + h; ++yy) {
        int rowBase = yy * a.Width;
        for (int xx = cx; xx < right; ++xx) {
          if (a.Pixels[rowBase + xx] >= 128) { ++ink; }
        }
      }
      sb.Append(ink / 4).Append('.');
    }
    return sb.ToString();
  }

  // Splits the band [y0, y1) into fixed-width columns, marks a column as moving
  // when enough of its pixels change across any pair of the supplied frames, then
  // returns the pixel extent of every separated cluster of moving columns as
  // [start0, endExclusive0, start1, endExclusive1, ...].
  //
  // The extents are what let a caller measure each character's own band instead of
  // hard-coding where the composition happens to put it: a cluster IS one
  // character's animated envelope, because a character hidden inside another's
  // silhouette cannot contribute a separated cluster of its own.
  public static int[] MovingColumnRanges(ShowcaseFrame[] frames, int y0, int y1,
      int columnWidth, int tolerance, double minChangedFraction, int minGapColumns) {
    if (frames == null || frames.Length < 2) { return new int[0]; }
    int width = frames[0].Width;
    int columns = width / columnWidth;
    bool[] moving = new bool[columns];
    int bandHeight = y1 - y0;
    for (int c = 0; c < columns; ++c) {
      int x = c * columnWidth;
      double best = 0.0;
      for (int i = 0; i + 1 < frames.Length; ++i) {
        for (int j = i + 1; j < frames.Length; ++j) {
          double ratio = DiffRatio(frames[i], frames[j], x, y0, columnWidth, bandHeight, tolerance);
          if (ratio > best) { best = ratio; }
        }
      }
      moving[c] = best >= minChangedFraction;
    }

    System.Collections.Generic.List<int> ranges = new System.Collections.Generic.List<int>();
    int gap = minGapColumns;
    for (int c = 0; c < columns; ++c) {
      if (moving[c]) {
        if (gap >= minGapColumns) {
          ranges.Add(c * columnWidth);
          ranges.Add((c + 1) * columnWidth);
        }
        else {
          ranges[ranges.Count - 1] = (c + 1) * columnWidth;
        }
        gap = 0;
      }
      else {
        ++gap;
      }
    }
    return ranges.ToArray();
  }

  // Number of separated clusters of moving columns in the band.
  public static int MovingColumnClusters(ShowcaseFrame[] frames, int y0, int y1,
      int columnWidth, int tolerance, double minChangedFraction, int minGapColumns) {
    return MovingColumnRanges(frames, y0, y1, columnWidth, tolerance, minChangedFraction, minGapColumns).Length / 2;
  }

  // The vertical counterpart of MovingColumnRanges, but answering a different
  // question. That method separates each character's own horizontal envelope by
  // clustering with gaps; this one measures how tall the WHOLE cast's silhouette
  // is, top to bottom, across a single pair of frames. A gap between the
  // nearest character's feet and the farthest character's knees is exactly as
  // much a part of the cast's vertical footprint as the rows themselves are, so
  // there is no clustering here: the answer is one bounding box, not several.
  //
  // Computed the same way as the column scan: split the frame into fixed-height
  // rows, mark a row moving when its changed-pixel fraction (at the caller's
  // tolerance) meets minChangedFraction, and return the pixel extent
  // [firstRow, lastRowExclusive] spanning every row that qualified. Returns an
  // empty array if no row met the threshold at all.
  public static int[] MovingRowExtent(ShowcaseFrame a, ShowcaseFrame b, int x0, int x1,
      int rowHeight, int tolerance, double minChangedFraction) {
    if (a.Width != b.Width || a.Height != b.Height) { return new int[0]; }
    int rows = a.Height / rowHeight;
    int bandWidth = x1 - x0;
    int first = -1;
    int last = -1;
    for (int r = 0; r < rows; ++r) {
      int y = r * rowHeight;
      double ratio = DiffRatio(a, b, x0, y, bandWidth, rowHeight, tolerance);
      if (ratio >= minChangedFraction) {
        if (first < 0) { first = y; }
        last = y + rowHeight;
      }
    }
    if (first < 0) { return new int[0]; }
    return new int[] { first, last };
  }

  // Per-cell motion inside a region: for each of cellsX*cellsY sub-rectangles, the
  // fraction of its pixels whose luminance changed between the two frames. This
  // describes how a region moved rather than what it looks like, so the profiles of
  // two characters standing at different places in the frame are comparable.
  public static double[] MotionProfile(ShowcaseFrame a, ShowcaseFrame b,
      int x, int y, int w, int h, int cellsX, int cellsY, int tolerance) {
    // Both frames, not just the first: DiffRatio reads the region out of each of
    // them, so checking only 'a' would leave the second read unguarded.
    Require(a, x, y, w, h);
    Require(b, x, y, w, h);
    if (cellsX <= 0 || cellsY <= 0 || w < cellsX || h < cellsY) {
      throw new ArgumentOutOfRangeException("cells", string.Format(
        "a {0}x{1} region cannot be split into {2}x{3} cells", w, h, cellsX, cellsY));
    }
    double[] profile = new double[cellsX * cellsY];
    for (int cy = 0; cy < cellsY; ++cy) {
      int top = y + (int)((long)h * cy / cellsY);
      int bottom = y + (int)((long)h * (cy + 1) / cellsY);
      for (int cx = 0; cx < cellsX; ++cx) {
        int left = x + (int)((long)w * cx / cellsX);
        int right = x + (int)((long)w * (cx + 1) / cellsX);
        profile[cy * cellsX + cx] = DiffRatio(a, b, left, top, right - left, bottom - top, tolerance);
      }
    }
    return profile;
  }

  // Total-variation distance between two motion profiles: each is normalised to sum
  // 1 and half the L1 difference is taken, giving 0.0 when the two regions spread
  // their motion over exactly the same cells in the same proportions and 1.0 when
  // they share no moving cell at all.
  //
  // Normalising is what makes the comparison about the motion rather than its size:
  // a character nearer the camera sweeps more pixels than one further away, and that
  // difference must not be mistaken for a difference in what they are doing. A
  // region that did not move at all normalises to zeros, so two frozen regions score
  // 0.0 and a frozen region against a moving one scores 0.5.
  public static double MotionShapeDistance(double[] profileA, double[] profileB) {
    if (profileA == null || profileB == null || profileA.Length != profileB.Length) {
      throw new ArgumentException("motion profiles must be the same length");
    }
    double totalA = 0.0;
    double totalB = 0.0;
    for (int i = 0; i < profileA.Length; ++i) { totalA += profileA[i]; totalB += profileB[i]; }
    double sum = 0.0;
    for (int i = 0; i < profileA.Length; ++i) {
      double a = (totalA > 0.0) ? profileA[i] / totalA : 0.0;
      double b = (totalB > 0.0) ? profileB[i] / totalB : 0.0;
      sum += Math.Abs(a - b);
    }
    return sum / 2.0;
  }
}
"@
}

try { [void][ShowcaseWin32]::SetProcessDPIAware() } catch { }

$HWND_TOPMOST = [IntPtr]::new(-1)
$HWND_NOTOPMOST = [IntPtr]::new(-2)
$SWP_NOSIZE = 0x0001
$SWP_NOMOVE = 0x0002
$SWP_SHOWWINDOW = 0x0040

$manifest = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'tools/readme_media_manifest.json') | ConvertFrom-Json
$project36 = @($manifest.projects | Where-Object { $_.number -eq '36' })[0]
if ($null -eq $project36) { throw 'project 36 is missing from tools/readme_media_manifest.json' }

$clientWidth = [int]$manifest.captureWidth
$clientHeight = [int]$manifest.captureHeight
$runtimeDir = Join-Path $repoRoot ([string]$manifest.runtimeDir)
$exePath = Join-Path $runtimeDir ([string]$project36.exe)
$projectDir = Join-Path $repoRoot 'Dx11/36_AdvancedAnim_Sound_Click'
$msbuildCommand = "& 'C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' 'Dx11\36_AdvancedAnim_Sound_Click\36_AdvancedAnim_Sound_Click.vcxproj' /t:Rebuild /p:Configuration=Debug /p:Platform=x64 /m:1"

# Showcase sampling schedule, in seconds after the scene goes live. The first two
# are the motion pair: every band is compared against itself across exactly this
# interval, so the four measurements describe the same half second of the show.
$slotSampleA = 0.6
$slotSampleB = 1.1
$driftSampleA = 2.6
$driftSampleB = 3.1
$slotSampleGapSec = $slotSampleB - $slotSampleA

# The HUD is a fixed-geometry window anchored at client (24,24). The probe sits
# just inside it, so it samples panel and text only.
$hudX = 28
$hudY = 28
$hudW = 312
$hudH = 56
# ImGui draws the panel opaque at (0.04, 0.05, 0.08), which rasterises to
# luminance ~12, so 40 is a generous ceiling for "this pixel is the panel, not the
# scene".
#
# Measured, not guessed. On a frame with the HUD, this rectangle reads
# dark = 0.8948 / bright = 0.0315 (identical across all four samples - the panel is
# static). Sweeping 200 same-sized rectangles across the rest of that same frame -
# skybox, characters, ground, and every mixture of them - the highest dark ratio
# any of them reached was 0.1987, and not one satisfied both halves at once. The
# 0.80 floor therefore sits 4x above anything the scene produces while leaving the
# real HUD 0.09 of headroom.
$hudPanelLuminance = 40
$hudPanelDarkFloor = 0.80
$hudTextBrightFloor = 0.01

# The clip line-up's own row inside that panel. The HUD now prints three lines -
# title, line-up, active technique - at the 20 px font InitImGui() loads, so with
# ImGui's default 8 px window padding and 4 px item spacing the three text boxes
# occupy client rows 32-52, 56-76 and 80-100. Everything that asks "what is the
# line-up doing" therefore measures rows 54-78: that covers the middle box whole
# with a 2 px margin on each side, and cannot see either neighbour.
#
# This band, not the whole panel, is what the line-up assertions read. The
# technique line is REQUIRED to change while a set runs (it names the window the
# show is in), and the line-up is required not to; a probe that spanned both could
# only ever assert one of the two, and over the whole panel it would assert
# neither - the technique label alone would break the "line-up holds still"
# check on frames where the line-up is in fact perfectly still.
$lineUpY = $hudY + 26
$lineUpH = 24

# The third box - the active technique - on the same reasoning: rows 82-106 cover
# it whole with a margin on each side and cannot see the line-up above it or the
# panel edge at row 108 below it.
#
# This band is not compared against a calibrated ink value. It is read across the
# whole layer sweep and SELF-CALIBRATED: the sweep straddles the window, so the
# caption takes exactly two values over it, and the frames carrying the layer are
# the ones whose row carries more ink - "UPPER-BODY LAYER" is a longer string than
# "BASE", which is a fact about the two literals App_PortfolioShowcase.inl prints
# rather than a threshold this file has to keep in step with a font size. The
# midpoint between the sweep's own minimum and maximum is the cut, and the sweep
# has to produce a maximum at least $techniqueCaptionInkRatio times its minimum
# for the reading to be used at all - so a run whose sweep never left one caption,
# or one where the technique line stopped rendering, fails instead of quietly
# selecting every frame or none.
#
# Measured, identical on all six runs of both builds: "BASE" rows read 0.0088 and
# "UPPER-BODY LAYER" rows read 0.0236, a 2.68x step, and the flip landed on the
# same sampled frame every time.
$techniqueY = $hudY + 54
$techniqueH = 24
$techniqueCaptionInkRatio = 1.5

# Technique windows, in seconds inside each 12 s set (App_PortfolioShowcase.inl):
# cross-fade 0.0-0.6 and upper-body layer 4.0-7.0. The two do not overlap, so every
# sample below reads at most one technique. A CCD IK window at 8.0-11.4 was removed
# from the runtime, and its reach-line assertion with it, because the solve fought
# the baked clip driving the same arm; set time 7.0-12.0 is now plain base clip.
#
# Set-boundary step. $boundaryStep is the mean absolute pixel difference between
# the frames straddling the first set change at t = 12 s, and $medianStep is the
# median of that same difference over ten consecutive mid-set pairs at the same
# spacing.
#
# The gap has to be short relative to the 0.6 s cross-fade, and the shorter the
# better. A hard clip switch dumps the entire difference between two unrelated
# poses into whichever pair contains it, however short that pair is. A cross-fade
# delivers only smoothstep(gap/0.6) of it - 0.24 across a 0.2 s straddle but 0.07
# across a 0.1 s one, because smoothstep leaves the origin quadratically - while
# an ordinary step shrinks only linearly with the gap. So halving the gap roughly
# halves what an honest fade contributes relative to a normal step, and does
# nothing at all to help a cut. (Measured at 0.2 s: 7.06 for a correct fade
# against a 3.25 median - a fade that reads as 2.2x an ordinary step purely
# because the pair is wide enough to contain a quarter of it.)
#
# The straddling pair is FOUND, not assumed. The burst runs across the boundary
# at $stepGapSec and the pair taken is the one whose HUD clip line-up changes
# across it, which is by construction the pair the set change falls inside -
# whatever the residual offset between this stopwatch and the show's own clock.
# An assumed pair that missed the boundary would compare two ordinary frames and
# pass vacuously.
$boundaryBurstStart = 11.75
$boundaryBurstCount = 7
$stepGapSec = 0.1
$stepBurstStart = 25.0
$stepBurstCount = 11
# Measured over the scene rows only. The HUD sits at client rows 24-108 and its
# line-up genuinely changes at the boundary, so including it would credit the
# boundary step with a caption change that is not the cast snapping.
$stepRegionTop = 120

# Upper-body layer. The layer runs on slot 0 alone and CharacterAnimator masks it
# to the spine/neck/head/arm chain, so it plays a second clip from the waist up
# while the base clip keeps driving the legs.
#
# The measurement is a RATIO TO A CONTROL: slot 0's arm-band change across half a
# second, divided by the mean of the same rows' change on the three companion
# slots in the same two frames. The companions never receive the layer, so they
# are a control for everything slot 0 shares with them - the lighting, the shadow
# softening, the exposure, the capture instant, the camera, the frame. A re-light
# that makes bodies shift harder against the ground moves numerator and
# denominator together and the ratio barely notices; that is exactly what the
# previous version of this measurement could not do.
#
# What it replaced, and why. The old metric divided slot 0's arm band by slot 0's
# OWN leg band and asserted the quotient cleared 1.10. Measured against a scratch
# build with `desc.upper.enabled = false` and nothing else changed, that same
# quotient over set 0's whole window reads 1.15-1.16 with the layer and 0.90-0.91
# without it - a ~27% gap that ordinary clip content swings further than the layer
# does, and one the toon-banded re-lighting closed entirely (it landed at 0.974,
# under its own floor, on a build whose layer is provably intact). The two pairs
# it actually sampled did not even agree with each other (0.74 and 1.208). It was
# measuring which clip was playing, not whether the layer was running - and its
# denominator, slot 0's own legs, is precisely the band the re-lighting inflated.
#
# WHERE it is measured matters as much as what. The layer changes WHERE the arms
# go, not how far they travel, so it is only legible where the base clip is not
# already flinging the same arms around. Measured over both sets with the scratch
# build, the best separation any variant of this statistic reaches inside set 0's
# window is 1.26x - the base clip there (VRM_1) spins slot 0 through a full turn
# across 4-7 s and drowns everything. Set 1's window is the opposite: slot 0 plays
# VRM_5, which is nearly still through 4-7 s, so the layer is the only thing
# moving that upper body. The layer code has no per-set branch - it is
# `isMainSlot && setTime >= 4 && setTime < 7` on every set - so measuring set 1's
# window exercises exactly the same path, with a signal that can actually be read.
#
# The rows are the arm span (250-340), NOT the whole torso: rows 150-250 are head
# and hair, which sway under the base clip on every frame and swamped the arm
# signal (with them included the old statistic read 0.581 without the layer and
# 0.576 with it). The SAME rows are read on the companions, so a lighting change
# that stirs one horizontal band of the frame stirs it for all four characters.
#
# The statistic over those pairs is the LOWER QUARTILE of the per-pair ratios, not
# the mean and not the median. What the layer guarantees is that slot 0's upper
# body never goes quiet while the window is open - it is being driven by a second
# clip through an alpha that only touches zero at the two ends - so the claim
# lives in the bottom tail. With the layer removed the per-pair ratios are
# strongly bimodal (about half land at 0.05-0.22 where VRM_5 is still, the rest at
# 0.4-1.0 where it is not), which puts the MEDIAN right on the boundary between
# the two lumps and makes it swing 0.22-0.45 on the count of sampled pairs alone.
# The quartile sits in the low lump, and every order statistic from the 2nd to the
# 7th smallest of 14 separates by 4.4-5.6x, so the choice is a plateau rather than
# a perch - the 8th, the median, is the first one that falls off it.
#
# Measured, three runs of each state, scratch build vs shipped build:
#
#   layer present:  0.747 / 0.755 / 0.753
#   layer removed:  0.110 / 0.122 / 0.133
#
# $layerControlRatioFloor = 0.30 is the geometric midpoint of that gap: the layer
# reads 2.5x above it and the layer-removed build 2.3x below it. Re-measured with
# the first three and the last three sampled pairs dropped - a stand-in for the
# sample window sliding 0.6 s against the show's clock - the layered runs never
# fall below 0.521 and the unlayered ones never rise above 0.133, so the floor
# keeps ~1.7x of margin on the side that matters even then.
$layerUpperTop = 250
$layerUpperBottom = 340
$layerControlRatioFloor = 0.30

# The sweep the ratio is taken over: set 1's layer window, bracketed on both sides
# by base-clip frames. 15.6-19.4 s is set time 3.6-7.4, so the 4.0-7.0 window sits
# inside it with two spare frames at each end - which is what lets the frames be
# selected by the HUD's own technique caption (below) instead of by this
# stopwatch, and what proves the selection discriminates at all.
$layerSweepStartSec = 15.6
$layerSweepEndSec = 19.4
$layerSweepStepSec = 0.2
# 14 consecutive in-window pairs were selected on every one of the six measured
# runs. 10 leaves room for the caption to flip a frame or two earlier or later
# than this stopwatch expects while still failing loudly - rather than measuring a
# quartile of nothing - if the window has moved or closed.
$layerMinPairs = 10

# Band 260-500 is the torso/arm span shared by all four slots (the nearest head is
# at row ~134 and the farthest feet at row ~653). The composition spaces the four
# animated envelopes ~140 px apart, which is 7 empty 20 px columns, so
# minGapColumns = 4 carries real margin: three boundary columns would have to flip
# at once before two clusters could merge.
$bandTop = 260
$bandBottom = 500
$bandColumnWidth = 20
$bandTolerance = 24
$bandMovingFraction = 0.10
$bandMinGapColumns = 4

# Rendered-style guard. The navy-shoe probe uses only the foot band and a dark,
# saturated blue predicate, so the costume's cool toon shadow cannot impersonate
# the model-specific accent. Palette balance is measured only where pale costume
# pixels move between two frames: every slot must show the cool shadow at least
# once, every sample must retain some warm light, and strongly purple pixels must
# remain a small minority. Candidate count keeps all ratios non-vacuous.
$toonShoeTop = 580
$toonShoeBottom = 760
$toonBlueAccentMinimum = 200
$toonCostumeTop = 430
$toonCostumeBottom = 700
$toonMotionColorTolerance = 24
$toonPaletteCandidateMinimum = 200
$toonCoolShadowMinimum = 0.30
$toonWarmLightMinimum = 0.15
$toonStrongPurpleMaximum = 0.12

# Each band's half second of motion is described by a 6x6 grid of changed-pixel
# fractions, normalised so only its shape is compared (see MotionShapeDistance).
# Two bands count as showing different motion when at least a fifth of that motion
# lands in different cells. Four characters driven by one clip in lock-step land at
# 0.0; four different clips separate far above the threshold.
$slotMotionCellsX = 6
$slotMotionCellsY = 6
$slotMotionShapeThreshold = 0.20

# Cast height band: the human's ruling on the four-characters-large vs.
# nothing-clips tradeoff, encoded as a number instead of left to drift. Four
# characters at 70% frame height need ~2129 px of width for the worst-case-wide
# ones out of this 1600 px frame - they clear the edges only around 53%. The
# chosen composition sits at ~60%, confirmed by eye at ~57%. 55-65% is a real
# band around that, not a rubber stamp: it fails on a composition change that
# meaningfully grows or shrinks the cast while tolerating normal per-clip sway.
$castHeightMinFraction = 0.55
$castHeightMaxFraction = 0.65

# MovingRowExtent's own row-scan parameters. Declared as their own constants
# rather than reused from the column scan just above, because that scan's
# $bandColumnWidth/$bandMovingFraction answer a different question (a 20 px-wide
# column over a 240 px torso/arm band) and retuning them would otherwise
# silently re-quantise this row-over-full-width measurement too.
#
# $castRowHeight only ever coincided with $bandColumnWidth's value (20); it has
# nothing to do with columns.
#
# $castRowMovingFraction cannot reuse $bandMovingFraction's value (0.10) even
# though it reuses its meaning: the column scan's denominator is one 20 px-wide
# column over a 240 px torso/arm band (4,800 px), while a row's denominator here
# is the full 1,600 px width, ~5x larger. Against that much bigger denominator, a
# single limb's sway - a hand, a swaying head, a stepping foot - never reaches
# 10% of the row, so 0.10 collapsed the measured extent to just the densest
# torso rows (260-440, 20% of frame height) and missed the actual head-to-feet
# envelope entirely.
# 0.005 was chosen by measuring the per-row fraction directly (diagnostic run
# kept in the task-3 report): every row from y=180 through y=700 carries
# 0.0069-0.21 of real motion, every row at y<=160 and y>=740 carries exactly 0,
# and the values step distinctly through that gap rather than hovering near
# 0.005 - so small shifts in the exact crossing row move the measured fraction
# by only a percentage point or two, not enough to threaten either band edge. At
# 0.005 the extent lands at rows 180-720, exactly 60% of the 900 px frame - dead
# center of the required 0.55-0.65 band. This is the most drift-sensitive of the
# four cast/edge thresholds on this page, which is why it is declared here
# beside the others instead of ~620 lines below at its point of use.
$castRowHeight = 20
$castRowMovingFraction = 0.005

# No body at a frame edge: the relaxed half of the same ruling. Finger tips and
# cloth/spring bones may brush an edge briefly; a torso, limb, or head may not.
# Measured directly on this composition across six consecutive frame pairs
# spanning both 12 s sets - slot-a/slot-b plus the five $cycleFrames pairs at
# t=4/6/8s (cycle 0) and t=14/16/18/22s (cycle 1) - not just a single half second
# near t=1s (see the assertion below and the task-3 report for the per-pair,
# per-edge numbers): all four edges - both full-height side columns and both
# full-width top/bottom rows - came in at exactly 0.0 on every one of those six
# pairs, reproduced identically on repeated runs. 0.02 (2% of an edge line) sits
# with essentially unlimited margin above that measured 0 - room for an
# occasional fingertip or sleeve tip to register a handful of pixels on a
# different sampled instant - while staying far below the several-percent run of
# changed pixels a body's width crossing an edge would actually produce.
$edgeBodyThreshold = 0.02

# The single-line probe above is maximally sensitive to a capture rectangle
# landing one pixel off: this test has been seen to fail with top=1, left=0.0067,
# right=0.01, bottom=0.0062 and then pass with all four at exactly 0 on the very
# next run of the identical binary - one edge line completely different while the
# other three merely shifted, which is a screen-capture alignment artifact, not a
# body. A body crossing an edge occupies a thick region; a one-pixel offset only
# ever disturbs the outermost line of pixels and leaves the pixels one or more
# steps further in reading whatever the show already has there (typically ~0).
# So each edge is measured at $edgeBandDepth successive single-pixel depths
# stepping inward from the physical edge, and the MINIMUM across those depths is
# what gets compared against $edgeBodyThreshold - not the outermost depth alone,
# and not an average, which a 25%-of-one-line spike could still push over 0.02
# after only widening the sampled region. A one-pixel offset spikes depth 0 and
# leaves depths 1..3 near 0, so the minimum collapses back to ~0. A body is many
# times wider than 4 px, so every depth in the band changes together and the
# minimum stays high. $edgeBodyThreshold itself is unchanged: this only changes
# how robustly it is measured, not what value it takes.
$edgeBandDepth = 4

# Skybox variety. The showcase renders a photographic outdoor scene above the
# horizon - stone ruins, foliage, a bright opening - which spans a wide range of
# luminance values; earlier in this project's history the same region rendered
# as a flat grey fill, which collapses to a single luminance value everywhere.
# DistinctLuminanceCount is the discriminator because it reads variety rather
# than brightness: a uniformly bright or uniformly dark fill still collapses to
# one value, so a regression that merely changed the fill's shade could not
# satisfy this the way it could satisfy a brightness-based check.
#
# Region: client columns 400-1590, rows 120-380 - clear of the HUD panel (client
# cols 24-344, rows 24-108) and safely above the ~430 px horizon on every sampled
# frame, since the skybox and ground never move once the scene is live.
#
# Measured on slot-a of a real run: 256 distinct luminance values over this
# region - the photographic scene spans the whole 0-255 luminance range. A flat
# fill reads 1 (or a small handful, if the region happened to straddle an
# antialiased edge). 40 sits 6.4x below the measured 256 while still leaving a
# flat-fill regression a 40x gap it cannot clear, so the floor has room on both
# sides: comfortably below what the real scene produces, and comfortably above
# anything a solid fill could produce.
$skyX = 400
$skyY = 120
$skyW = 1190
$skyH = 260
$skyDistinctLuminanceFloor = 40

# ---------------------------------------------------------------------------
# 1. The built binary must exist and must be newer than the sources it is built
#    from. Never silently skip.
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $exePath -PathType Leaf)) {
    throw "Built executable not found: $exePath`nBuild it first:`n$msbuildCommand"
}

$sourceFiles = @()
$sourceFiles += Get-ChildItem -LiteralPath $projectDir -File |
    Where-Object { $_.Extension -in @('.cpp', '.h', '.inl', '.vcxproj', '.filters', '.hlsl', '.hlsli', '.fxh') }
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Dx11/Common/Animation') -File |
    Where-Object { $_.Extension -in @('.cpp', '.h') }

$exeWriteTime = (Get-Item -LiteralPath $exePath).LastWriteTimeUtc
$staleSources = @($sourceFiles | Where-Object { $_.LastWriteTimeUtc -gt $exeWriteTime })
if ($staleSources.Count -gt 0) {
    $names = ($staleSources | ForEach-Object { $_.Name }) -join ', '
    throw "Built executable is older than its sources ($names).`nRebuild it first:`n$msbuildCommand"
}

Write-Host "  ok   built binary is present and newer than its sources"

$lifecycleSource = Get-Content -Raw -LiteralPath (Join-Path $projectDir 'App_Lifecycle.inl')
$renderPassSource = Get-Content -Raw -LiteralPath (Join-Path $projectDir 'App_RenderPasses.inl')
$panelSource = Get-Content -Raw -LiteralPath (Join-Path $projectDir 'App_ImGuiPanels.inl')
$toonPbrIsWired = ($lifecycleSource -match 'model->modelShading\s*=\s*ShadingMode::PBR\s*;') -and
    ($lifecycleSource -match 'model->useToonShading\s*=\s*true\s*;') -and
    ($lifecycleSource -notmatch 'model->modelShading\s*=\s*ShadingMode::BlinnPhong\s*;')
Assert-True $toonPbrIsWired 'the showcase models select PBR plus the per-model toon flag, making anime ToonPBR reachable'
Assert-True (($lifecycleSource -match 'const\s+XMFLOAT3\s+characterScale\s*\(\s*80\.0f\s*,\s*80\.0f\s*,\s*80\.0f\s*\)') -and
    ($lifecycleSource -match 'm_Camera\.SetSpeed\s*\(\s*40\.0f\s*\)')) `
    'the four-character composition keeps its intentional scale 80 and responsive camera speed 40'
foreach ($panelCall in @(
        'RenderControlPannel', 'RenderSceneCollection', 'RenderModelPannel', 'RenderQuickGuideUI',
        'RenderAdvancedRigUI', 'RenderConsolPannel', 'RenderSceneImageWindow', 'RenderDeferredUI',
        'RenderSoundDebugUI')) {
    Assert-True ($renderPassSource -match "\b$panelCall\s*\(\s*\)") "ordinary UI wiring retains $panelCall"
}
Assert-True ($renderPassSource -match 'm_->m_SystemInfo\.RenderUI\s*\(\s*\)') `
    'ordinary UI wiring retains System Info'
Assert-True ($renderPassSource -match 'SkyboxAssetManager::RenderStatusUI\s*\(\s*\)') `
    'ordinary UI wiring retains Skybox status'
Assert-True ($renderPassSource -match 'm_->m_SniperEnabled\s*&&\s*m_->m_SniperCharging') `
    'ordinary UI wiring retains the sniper charge overlay'
Assert-True (($renderPassSource -match 'if\s*\(\s*readmeCaptureMode\s*\)') -and
    ($renderPassSource -match 'RenderPortfolioShowcaseHud\s*\(\s*\)')) `
    'README capture mode keeps the compact portfolio HUD path'
Assert-True (($panelSource -match 'Advanced Rig is unavailable in showcase mode') -and
    ($panelSource -match 'Unavailable in portfolio mode') -and
    ($panelSource -match 'Portfolio composition is locked to Forward')) `
    'ordinary UI explains the Advanced Rig and Deferred portfolio locks'
Assert-True (-not [ShowcaseFrame]::IsToonPaletteCandidateRgb(65, 75, 115)) `
    'the toon costume mask rejects a dark navy shoe pixel that otherwise satisfies its colour range'

# ---------------------------------------------------------------------------
# 2. Rights boundary. A provenance assertion about the bytes that ship, not a
#    change detector: the Project 36 deliverables must contain no trace of the
#    legacy licensed model, its named dance clips, or its audio.
# ---------------------------------------------------------------------------
$forbiddenTokens = @(
    'NIKKE',
    'Alice_.fbx',
    'CaramellaDansen',
    'RabbitHole',
    'Specialist',
    'CaliforniaGirls'
)

$deliverables = @()
$deliverables += Get-ChildItem -LiteralPath $projectDir -File -Recurse |
    Where-Object { $_.Extension -in @('.cpp', '.h', '.inl', '.md', '.vcxproj', '.filters', '.hlsl', '.hlsli', '.fxh') }

# The binary assets Project 36 actually ships and loads. Named one by one, and
# required to exist, rather than matched by a wildcard: the previous glob pointed
# at anim_Portfolio*.glb, which Task 1 deleted, so it silently matched nothing and
# the scan covered no binary at all while still reporting a pass. The showcase's
# animations now live inside SampleModel.glb, so that is where the provenance
# assertion has to look.
$shippedAssets = @(
    'Dx11/Resource/fbx/Public/MyAlice/Player/SampleModel.glb'
    'Dx11/Resource/fbx/Public/MyAlice/Enemy/AliceEnemy1.glb'
    'Dx11/Resource/fbx/Public/MyAlice/Enemy/AliceEnemy2.glb'
    'Dx11/Resource/fbx/Public/MyAlice/Enemy/AliceEnemy3.glb'
    'Dx11/Resource/fbx/Public/MyAlice/Animations/anim_Idle.fbx'
)
$binaryCount = 0
foreach ($relativeAsset in $shippedAssets) {
    $assetPath = Join-Path $repoRoot $relativeAsset
    if (-not (Test-Path -LiteralPath $assetPath -PathType Leaf)) {
        throw "rights boundary scan cannot find a shipped asset it must cover: $relativeAsset"
    }
    $deliverables += Get-Item -LiteralPath $assetPath
    ++$binaryCount
}

foreach ($deliverable in $deliverables) {
    $bytes = [System.IO.File]::ReadAllBytes($deliverable.FullName)
    $asAscii = [System.Text.Encoding]::ASCII.GetString($bytes)
    foreach ($token in $forbiddenTokens) {
        if ($asAscii.IndexOf($token, [System.StringComparison]::OrdinalIgnoreCase) -ge 0) {
            $script:Failures.Add("legacy rights boundary violated: '$token' found in $($deliverable.Name)")
        }
    }
}
Write-Host "  ok   rights boundary scan covered $($deliverables.Count) Project 36 deliverables ($binaryCount shipped binaries)"

# ---------------------------------------------------------------------------
# Runtime helpers
# ---------------------------------------------------------------------------
function Get-ShowcaseWindowTitle {
    param([IntPtr]$Handle)

    $length = [ShowcaseWin32]::GetWindowTextLength($Handle)
    if ($length -le 0) { return '' }
    $builder = New-Object System.Text.StringBuilder($length + 1)
    [void][ShowcaseWin32]::GetWindowText($Handle, $builder, $builder.Capacity)
    return $builder.ToString()
}

function Resolve-ShowcaseWindow {
    param([System.Diagnostics.Process]$Process)

    $candidates = New-Object System.Collections.Generic.List[object]
    $addCandidate = {
        param([IntPtr]$CandidateHandle)

        if ($CandidateHandle -eq [IntPtr]::Zero -or -not [ShowcaseWin32]::IsWindowVisible($CandidateHandle)) { return }
        $rect = New-Object ShowcaseWin32+RECT
        if (-not [ShowcaseWin32]::GetWindowRect($CandidateHandle, [ref]$rect)) { return }
        if (($rect.Right - $rect.Left) -le 32 -or ($rect.Bottom - $rect.Top) -le 32) { return }
        $candidates.Add([pscustomobject]@{ Handle = $CandidateHandle; Title = (Get-ShowcaseWindowTitle -Handle $CandidateHandle) })
    }

    $Process.Refresh()
    if ($Process.MainWindowHandle -ne [IntPtr]::Zero) { & $addCandidate ([IntPtr]$Process.MainWindowHandle) }

    $callback = [ShowcaseWin32+EnumWindowsProc]{
        param([IntPtr]$WindowHandle, [IntPtr]$LParam)

        $windowProcessId = 0
        [void][ShowcaseWin32]::GetWindowThreadProcessId($WindowHandle, [ref]$windowProcessId)
        if ($windowProcessId -eq $Process.Id) { & $addCandidate $WindowHandle }
        return $true
    }
    [void][ShowcaseWin32]::EnumWindows($callback, [IntPtr]::Zero)

    if ($candidates.Count -eq 0) { return $null }
    $preferred = @($candidates | Where-Object { $_.Title -eq 'GameApp' } | Select-Object -First 1)
    if ($preferred.Count -eq 0) { $preferred = @($candidates | Where-Object { -not [string]::IsNullOrWhiteSpace($_.Title) } | Select-Object -First 1) }
    if ($preferred.Count -eq 0) { return $candidates[0] }
    return $preferred[0]
}

function Wait-ShowcaseWindow {
    param([System.Diagnostics.Process]$Process, [int]$TimeoutMs = 30000)

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { throw "process exited before a main window appeared (exit $($Process.ExitCode))" }
        $window = Resolve-ShowcaseWindow -Process $Process
        if ($null -ne $window) { return $window }
        Start-Sleep -Milliseconds 250
    } while ((Get-Date) -lt $deadline)

    throw 'timed out waiting for the application main window'
}

function Set-ShowcaseClientSize {
    param([IntPtr]$Handle, [int]$Width, [int]$Height)

    for ($attempt = 0; $attempt -lt 5; ++$attempt) {
        $window = New-Object ShowcaseWin32+RECT
        $client = New-Object ShowcaseWin32+RECT
        if ([ShowcaseWin32]::GetWindowRect($Handle, [ref]$window) -and [ShowcaseWin32]::GetClientRect($Handle, [ref]$client)) {
            $outerWidth = $Width + (($window.Right - $window.Left) - ($client.Right - $client.Left))
            $outerHeight = $Height + (($window.Bottom - $window.Top) - ($client.Bottom - $client.Top))
            [void][ShowcaseWin32]::SetWindowPos($Handle, [IntPtr]::Zero, 40, 40, $outerWidth, $outerHeight, [uint32]$SWP_SHOWWINDOW)
            Start-Sleep -Milliseconds 200
            $resized = New-Object ShowcaseWin32+RECT
            if ([ShowcaseWin32]::GetClientRect($Handle, [ref]$resized)) {
                if (($resized.Right - $resized.Left) -eq $Width -and ($resized.Bottom - $resized.Top) -eq $Height) { return }
            }
        }
        Start-Sleep -Milliseconds 250
    }

    throw "unable to resize the application client area to ${Width}x${Height}"
}

function Set-ShowcaseTopmost {
    param([IntPtr]$Handle)

    $flags = [uint32]($SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_SHOWWINDOW)
    [void][ShowcaseWin32]::SetWindowPos($Handle, $HWND_TOPMOST, 0, 0, 0, 0, $flags)
    [void][ShowcaseWin32]::ShowWindow($Handle, 9)
    [void][ShowcaseWin32]::SetForegroundWindow($Handle)
    [void][ShowcaseWin32]::BringWindowToTop($Handle)
}

function Reset-ShowcaseTopmost {
    param([IntPtr]$Handle)

    if ($Handle -eq [IntPtr]::Zero) { return }
    $flags = [uint32]($SWP_NOMOVE -bor $SWP_NOSIZE -bor $SWP_SHOWWINDOW)
    [void][ShowcaseWin32]::SetWindowPos($Handle, $HWND_NOTOPMOST, 0, 0, 0, 0, $flags)
}

function Get-ShowcaseClientOrigin {
    param([IntPtr]$Handle)

    $origin = New-Object ShowcaseWin32+POINT
    if (-not [ShowcaseWin32]::ClientToScreen($Handle, [ref]$origin)) { throw 'unable to map the client origin to screen coordinates' }
    return $origin
}

function Save-ShowcaseFrame {
    param([IntPtr]$Handle, [int]$Width, [int]$Height, [string]$Path)

    $origin = Get-ShowcaseClientOrigin -Handle $Handle
    $bitmap = $null
    $graphics = $null
    $bitmapData = $null
    try {
        $bitmap = New-Object System.Drawing.Bitmap($Width, $Height)
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        $graphics.CopyFromScreen($origin.X, $origin.Y, 0, 0, $bitmap.Size)
        if (-not [string]::IsNullOrEmpty($Path)) {
            $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
        }

        $rect = New-Object System.Drawing.Rectangle(0, 0, $Width, $Height)
        $bitmapData = $bitmap.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $byteCount = $bitmapData.Stride * $Height
        $buffer = New-Object byte[] $byteCount
        [System.Runtime.InteropServices.Marshal]::Copy($bitmapData.Scan0, $buffer, 0, $byteCount)
        return [ShowcaseFrame]::FromBgra($buffer, $bitmapData.Stride, $Width, $Height)
    }
    finally {
        if ($null -ne $bitmapData -and $null -ne $bitmap) { $bitmap.UnlockBits($bitmapData) }
        if ($null -ne $graphics) { $graphics.Dispose() }
        if ($null -ne $bitmap) { $bitmap.Dispose() }
    }
}

function Send-ShowcaseClick {
    param([IntPtr]$Handle, [int]$ClientX, [int]$ClientY)

    $lParam = [IntPtr]((($ClientY -band 0xffff) -shl 16) -bor ($ClientX -band 0xffff))
    [void][ShowcaseWin32]::PostMessage($Handle, [ShowcaseWin32]::WM_LBUTTONDOWN, [IntPtr][ShowcaseWin32]::MK_LBUTTON, $lParam)
    Start-Sleep -Milliseconds 40
    [void][ShowcaseWin32]::PostMessage($Handle, [ShowcaseWin32]::WM_LBUTTONUP, [IntPtr]::Zero, $lParam)
}

# The loading screen clears the whole back buffer to pure black; the live scene
# fills the corners with the skybox. That makes corner brightness an independent
# readiness signal that does not depend on the feature under test.
function Wait-ShowcaseScene {
    param(
        [System.Diagnostics.Process]$Process,
        [IntPtr]$Handle,
        [int]$Width,
        [int]$Height,
        [string]$ProbePath,
        [switch]$ClickToStart,
        [int]$TimeoutMs = 120000
    )

    $deadline = (Get-Date).AddMilliseconds($TimeoutMs)
    do {
        $Process.Refresh()
        if ($Process.HasExited) { throw "process exited while waiting for the scene (exit $($Process.ExitCode))" }
        if ($ClickToStart) { Send-ShowcaseClick -Handle $Handle -ClientX ([int]($Width / 2)) -ClientY ([int]($Height / 2)) }
        Start-Sleep -Milliseconds 700
        $frame = Save-ShowcaseFrame -Handle $Handle -Width $Width -Height $Height -Path $ProbePath
        # Bottom-left corner: outside the centred loading window in every state.
        $lit = [ShowcaseFrame]::BrightRatio($frame, 0, $Height - 140, 160, 120, 24)
        if ($lit -ge 0.5) { return }
    } while ((Get-Date) -lt $deadline)

    throw 'timed out waiting for the live scene to replace the loading screen'
}

function Stop-ShowcaseProcess {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) { return }
    try {
        $Process.Refresh()
        if (-not $Process.HasExited) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            [void]$Process.WaitForExit(8000)
        }
    }
    catch { }
}

# Ask the window to close and let the application run its own shutdown, falling
# back to a forced kill. A publisher that cleans up after itself can only be
# observed doing so if it is given the chance to shut down.
function Close-ShowcaseProcess {
    param([System.Diagnostics.Process]$Process, [IntPtr]$Handle, [int]$TimeoutMs = 20000)

    if ($null -eq $Process) { return $true }
    try {
        $Process.Refresh()
        if ($Process.HasExited) { return $true }
        if ($Handle -ne [IntPtr]::Zero) {
            [void][ShowcaseWin32]::PostMessage($Handle, [ShowcaseWin32]::WM_CLOSE, [IntPtr]::Zero, [IntPtr]::Zero)
            if ($Process.WaitForExit($TimeoutMs)) { return $true }
        }
    }
    catch { }
    Stop-ShowcaseProcess -Process $Process
    return $false
}

# Observes the published backbuffer PNG the way a correct consumer must: take the
# bytes under FileShare.ReadWrite, release the file immediately so the publisher's
# atomic replace is not blocked, then decode from memory. Decoding is forced all
# the way to raster pixels because a torn PNG still parses its header - only
# drawing it exposes the truncation.
#
# MoveFileExW(..., MOVEFILE_REPLACE_EXISTING) needs exclusive access to the
# destination for the instant of the rename, so a reader whose File.Open lands in
# that window gets ERROR_SHARING_VIOLATION / ERROR_LOCK_VIOLATION. That is the open
# failing, not a partial file: the reader caught nothing at all, which is exactly
# what a correct atomic replace predicts, so it is tracked as its own "contended"
# outcome instead of being folded into "torn". Everything else that fails to read
# or decode still counts as torn - that is the only evidence that would actually
# disprove atomicity.
#
# Observed=false, Contended=false -> the file does not exist yet (a legal
#                                     observation).
# Observed=false, Contended=true  -> the open lost a race with the publisher's
#                                     atomic rename; the reader observed nothing,
#                                     not a partial file.
# Observed=true,  Complete=false  -> the file was opened but a short read, a
#                                     decode failure, or a bad raster proved a
#                                     torn publication, which is exactly what
#                                     atomicity forbids.
# Observed=true,  Complete=true   -> a fully decoded frame.
function Read-ShowcaseBackbufferPng {
    param([string]$Path)

    $absent = [pscustomobject]@{ Observed = $false; Complete = $false; Contended = $false; Reason = ''; Width = 0; Height = 0; Signature = '' }
    $bytes = $null
    try {
        $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
        try {
            $bytes = New-Object byte[] ([int]$stream.Length)
            $offset = 0
            while ($offset -lt $bytes.Length) {
                $read = $stream.Read($bytes, $offset, $bytes.Length - $offset)
                if ($read -le 0) { break }
                $offset += $read
            }
            if ($offset -ne $bytes.Length) { throw "short read: $offset of $($bytes.Length) bytes" }
        }
        finally { $stream.Dispose() }
    }
    catch [System.IO.FileNotFoundException] { return $absent }
    catch [System.IO.DirectoryNotFoundException] { return $absent }
    catch [System.IO.IOException] {
        # MoveFileExW's replace briefly holds the destination exclusively; a reader
        # whose open lands in that window gets ERROR_SHARING_VIOLATION (0x20) or
        # ERROR_LOCK_VIOLATION (0x21). The open itself failed, so nothing was
        # observed - this is not evidence of a torn file and must not be counted
        # as one.
        $win32Error = $_.Exception.HResult -band 0xFFFF
        if ($win32Error -eq 0x20 -or $win32Error -eq 0x21) {
            return [pscustomobject]@{ Observed = $false; Complete = $false; Contended = $true
                Reason = "contended (0x$($win32Error.ToString('X2'))): $($_.Exception.Message)"
                Width = 0; Height = 0; Signature = '' }
        }
        return [pscustomobject]@{ Observed = $true; Complete = $false; Contended = $false
            Reason = "unreadable: $($_.Exception.Message)"; Width = 0; Height = 0; Signature = '' }
    }
    catch {
        return [pscustomobject]@{ Observed = $true; Complete = $false; Contended = $false
            Reason = "unreadable: $($_.Exception.Message)"; Width = 0; Height = 0; Signature = '' }
    }

    $memory = $null
    $image = $null
    $raster = $null
    $bits = $null
    try {
        $memory = New-Object System.IO.MemoryStream(, $bytes)
        $image = [System.Drawing.Image]::FromStream($memory, $false, $true)
        $raster = New-Object System.Drawing.Bitmap($image)
        $rect = New-Object System.Drawing.Rectangle(0, 0, $raster.Width, $raster.Height)
        $bits = $raster.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly,
            [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $pixelCount = $bits.Stride * $raster.Height
        $pixels = New-Object byte[] $pixelCount
        [System.Runtime.InteropServices.Marshal]::Copy($bits.Scan0, $pixels, 0, $pixelCount)
        $raster.UnlockBits($bits)
        $bits = $null
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try { $signature = [System.BitConverter]::ToString($sha.ComputeHash($pixels)) }
        finally { $sha.Dispose() }
        return [pscustomobject]@{ Observed = $true; Complete = $true; Contended = $false; Reason = ''
            Width = $raster.Width; Height = $raster.Height; Signature = $signature }
    }
    catch {
        return [pscustomobject]@{ Observed = $true; Complete = $false; Contended = $false
            Reason = "undecodable ($($bytes.Length) bytes): $($_.Exception.Message)"
            Width = 0; Height = 0; Signature = '' }
    }
    finally {
        if ($null -ne $bits -and $null -ne $raster) { $raster.UnlockBits($bits) }
        if ($null -ne $raster) { $raster.Dispose() }
        if ($null -ne $image) { $image.Dispose() }
        if ($null -ne $memory) { $memory.Dispose() }
    }
}

$frameDir = Join-Path ([System.IO.Path]::GetTempPath()) ("dx11-project36-showcase-{0}" -f $PID)
New-Item -ItemType Directory -Path $frameDir -Force | Out-Null

# Ignored staging directories for the opt-in backbuffer publisher. One per run so
# an assertion can never be satisfied by a file some other run left behind. There
# is deliberately no directory for the capture-mode-without-the-variable run
# below: no path is handed to that process at all, so a directory it was never
# told about could only ever be empty, whatever the implementation did.
$backbufferDir = Join-Path $frameDir 'backbuffer-capture'
$plainBackbufferDir = Join-Path $frameDir 'backbuffer-plain'
New-Item -ItemType Directory -Path $backbufferDir, $plainBackbufferDir -Force | Out-Null
$backbufferPath = Join-Path $backbufferDir 'project36-backbuffer.png'
$backbufferTempPath = $backbufferPath + '.tmp.png'
$plainBackbufferPath = Join-Path $plainBackbufferDir 'project36-plain.png'

$previousCaptureEnv = $env:DX11_README_CAPTURE
$previousBackbufferEnv = $env:DX11_README_BACKBUFFER_PNG
$showcaseProcess = $null
$interactiveProcess = $null
$captureProcess = $null
$gateProcess = $null
$showcaseHandle = [IntPtr]::Zero
$interactiveHandle = [IntPtr]::Zero
$captureHandle = [IntPtr]::Zero
$gateHandle = [IntPtr]::Zero

# ImGui windows remember their geometry in Dx11/bin/imgui.ini - untracked,
# gitignored, and rewritten whenever a developer drags a window. The assertions
# below measure fixed client rectangles, so they would drift with that unversioned
# local state. Set the run aside from it: stash the file in memory for the duration
# of the test so every process starts from the layout the C++ source actually
# specifies, then put the developer's file back exactly as it was.
$imguiIniPath = Join-Path $runtimeDir 'imgui.ini'
$imguiIniExisted = Test-Path -LiteralPath $imguiIniPath -PathType Leaf
$imguiIniBytes = $null
if ($imguiIniExisted) {
    $imguiIniBytes = [System.IO.File]::ReadAllBytes($imguiIniPath)
    Remove-Item -LiteralPath $imguiIniPath -Force
    Write-Host "  ok   stashed $imguiIniPath ($($imguiIniBytes.Length) bytes) so panel placement comes from the sources"
}
else {
    Write-Host '  ok   no imgui.ini present; panel placement comes from the sources'
}

try {
    # -----------------------------------------------------------------------
    # 3. README capture launch: the published media gets the compact showcase HUD
    #    and four characters, without the interactive editor windows covering the
    #    composition. Backbuffer publication remains disabled because no output
    #    path is supplied in this run.
    # -----------------------------------------------------------------------
    $env:DX11_README_CAPTURE = '1'
    Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue
    $showcaseProcess = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
    if ($null -eq $previousCaptureEnv) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $previousCaptureEnv }
    if ($null -eq $previousBackbufferEnv) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue }
    else { $env:DX11_README_BACKBUFFER_PNG = $previousBackbufferEnv }

    $showcaseWindow = Wait-ShowcaseWindow -Process $showcaseProcess
    $showcaseHandle = $showcaseWindow.Handle
    Set-ShowcaseClientSize -Handle $showcaseHandle -Width $clientWidth -Height $clientHeight
    Set-ShowcaseTopmost -Handle $showcaseHandle
    Start-Sleep -Milliseconds 400
    Wait-ShowcaseScene -Process $showcaseProcess -Handle $showcaseHandle -Width $clientWidth -Height $clientHeight `
        -ProbePath (Join-Path $frameDir 'showcase-probe.png')

    # Put the show back to t = 0 and start the sampling clock on the same instant.
    #
    # A left click calls ResetPortfolioShowcase(), which is the documented way to
    # re-watch the rotation from its opening. Without this, the only thing that
    # started the show was whichever click Wait-ShowcaseScene happened to land
    # last, ~700 ms plus a screen capture before it returned - so every sample
    # below was taken most of a second later in the show than its name says. That
    # slack did not matter while the only time-sensitive samples sat in the middle
    # of a 12 s set, but the technique windows are 0.6-3.4 s wide and the boundary
    # pair has to straddle a single instant, so the clocks now have to agree.
    #
    # Send-ShowcaseClick posts WM_LBUTTONDOWN, waits 40 ms, then posts the button
    # up; the reset fires on the down transition, one message pump apart. So by
    # the time it returns the show is ~40 ms old, and the stopwatch starts there.
    Send-ShowcaseClick -Handle $showcaseHandle -ClientX ([int]($clientWidth / 2)) -ClientY ([int]($clientHeight / 2))
    $showcaseClock = [System.Diagnostics.Stopwatch]::StartNew()
    $samples = [ordered]@{}
    foreach ($sample in @(
            @{ Name = 'slot-a'; At = $slotSampleA },
            @{ Name = 'slot-b'; At = $slotSampleB },
            @{ Name = 'drift-a'; At = $driftSampleA },
            @{ Name = 'drift-b'; At = $driftSampleB })) {
        $waitMs = [int](($sample.At * 1000.0) - $showcaseClock.Elapsed.TotalMilliseconds)
        if ($waitMs -gt 0) { Start-Sleep -Milliseconds $waitMs }
        $samples[$sample.Name] = Save-ShowcaseFrame -Handle $showcaseHandle -Width $clientWidth -Height $clientHeight `
            -Path (Join-Path $frameDir "showcase-$($sample.Name).png")
    }

    # Clip-rotation sampling. A set lasts 12 s, so t = 6 s sits in the middle of
    # cycle 0 and t = 18 s in the middle of cycle 1 - as far from a set boundary as
    # a sample can be, which is what makes the comparison robust to the second or so
    # of slack between the click that starts the show and this clock.
    # Clip-rotation and technique sampling, in one time-ordered schedule.
    #
    #   layer-a/layer-b  4.4 / 4.9 s - inside SET 0's 4.0-7.0 s upper-body layer,
    #                    where layerAlpha = sin(layer01 * pi) is climbing steeply
    #                    (0.446 -> 0.837). The layer assertion is NOT taken here:
    #                    slot 0's base clip in set 0 is VRM_1, which spins the whole
    #                    character through a turn across exactly this window, and no
    #                    variant of the measurement separates a layered upper body
    #                    from an unlayered one by more than 1.26x against that (see
    #                    $layerControlRatioFloor). The assertion is taken over set
    #                    1's window instead, where slot 0 plays the near-still
    #                    VRM_5; these two stills stay as stills - they feed the HUD
    #                    line-up assertions, which read EVERY sampled frame, and
    #                    they are what a reader looks at to see set 0's layer.
    #   cycle0-075 /     7.5 / 9.5 s - two base-clip stills in the second half of
    #   cycle0-095       set 0. Nothing but the base clips runs after 7.0 s (this is
    #                    where the removed CCD IK window used to sit), so these two
    #                    are kept for the HUD line-up assertions, which read EVERY
    #                    sampled frame, and as evidence for the author that slot 0's
    #                    arms now move like the other three across that stretch.
    #   boundary-a       11.5 s - the last still of cycle 0, one set-length of
    #                    negative-cycle outgoing-clip lookups after t = 0.
    $cycleFrames = [ordered]@{}
    $takeSamples = {
        param($Schedule)

        foreach ($sample in $Schedule) {
            $waitMs = [int](($sample.At * 1000.0) - $showcaseClock.Elapsed.TotalMilliseconds)
            if ($waitMs -gt 0) { Start-Sleep -Milliseconds $waitMs }
            $cycleFrames[$sample.Name] = Save-ShowcaseFrame -Handle $showcaseHandle -Width $clientWidth -Height $clientHeight `
                -Path (Join-Path $frameDir "showcase-$($sample.Name).png")
        }
    }
    # A burst with no PNG behind it. Saving a 1600x900 file per frame costs more
    # than the 0.1 s cadence allows, and these frames are a measurement rather
    # than evidence a human reads.
    $takeBurst = {
        param([double]$Start, [int]$Count)

        $burst = @()
        for ($burstIndex = 0; $burstIndex -lt $Count; ++$burstIndex) {
            $burstAt = $Start + ($burstIndex * $stepGapSec)
            $waitMs = [int](($burstAt * 1000.0) - $showcaseClock.Elapsed.TotalMilliseconds)
            if ($waitMs -gt 0) { Start-Sleep -Milliseconds $waitMs }
            $burst += Save-ShowcaseFrame -Handle $showcaseHandle -Width $clientWidth -Height $clientHeight
        }
        return , $burst
    }

    & $takeSamples @(
        @{ Name = 'cycle0-04'; At = 4.0 },
        @{ Name = 'layer-a'; At = 4.4 },
        @{ Name = 'layer-b'; At = 4.9 },
        @{ Name = 'cycle0-06'; At = 6.0 },
        @{ Name = 'cycle0-075'; At = 7.5 },
        @{ Name = 'cycle0-08'; At = 8.0 },
        @{ Name = 'cycle0-095'; At = 9.5 },
        @{ Name = 'boundary-a'; At = 11.5 })

    # Across the first set change, at the same cadence the mid-set reference burst
    # below uses. Seven frames spanning 11.75-12.35 s cover t = 12 s with three
    # frames of slack on each side, which is far more than the residual offset
    # between this stopwatch and the show's clock.
    $boundaryFrames = & $takeBurst $boundaryBurstStart $boundaryBurstCount

    & $takeSamples @(@{ Name = 'cycle1-14'; At = 14.0 })

    # Set 1's upper-body layer window, swept at $layerSweepStepSec so the ratio
    # below has a distribution to take a quartile of rather than two lucky pairs.
    # The sweep starts before the window opens and ends after it closes, which is
    # what lets the HUD's technique caption pick the in-window frames out of it.
    #
    # t = 16.0 s and t = 18.0 s fall on this grid and are the instants the
    # cycle1-16 / cycle1-18 samples already stood at, so the sweep keeps their
    # names at those two steps instead of capturing the same moment twice: the
    # frame-edge guard and the HUD-signature assertions go on reading exactly the
    # frames they read before.
    $layerSweepNames = @()
    $layerSweep = @()
    for ($sweepAt = $layerSweepStartSec; $sweepAt -le ($layerSweepEndSec + 0.0001); $sweepAt += $layerSweepStepSec) {
        $sweepTime = [Math]::Round($sweepAt, 2)
        $sweepName = switch ($sweepTime) {
            16.0 { 'cycle1-16' }
            18.0 { 'cycle1-18' }
            default { 'layer-{0:0000}' -f [int][Math]::Round($sweepTime * 100) }
        }
        $layerSweep += @{ Name = $sweepName; At = $sweepTime }
        $layerSweepNames += $sweepName
    }
    & $takeSamples $layerSweep

    & $takeSamples @(@{ Name = 'cycle1-22'; At = 22.0 })

    # The mid-set reference for $medianStep: eleven frames giving ten consecutive
    # pairs, at 25.0-26.0 s - set time 1.0-2.0 of cycle 2 - which is past the
    # cross-fade and well before the layer window, so every pair measures nothing
    # but four characters playing their base clips.
    $stepFrames = & $takeBurst $stepBurstStart $stepBurstCount
    $showcaseClock.Stop()

    $showcaseProcess.Refresh()
    Assert-True (-not $showcaseProcess.HasExited) 'the README showcase launch is still running after the sampled seconds'
    Assert-True ($showcaseProcess.Responding) 'the README showcase launch stays responsive while driving four palettes'
    Assert-True ($showcaseProcess.MainWindowHandle -ne [IntPtr]::Zero) 'the README showcase launch owns a visible main window'

    $slotA = $samples['slot-a']
    $slotB = $samples['slot-b']
    $driftA = $samples['drift-a']
    $driftB = $samples['drift-b']

    # The skybox renders a real photographic scene rather than a flat fill. See
    # $skyDistinctLuminanceFloor above for the region, the measured value, and the
    # margin: the count is of distinct luminance values, which reads variety
    # rather than brightness, so a regression back to a flat clear-colour fill
    # cannot pass merely by matching the scene's average brightness.
    $skyDistinctLuminance = [ShowcaseFrame]::DistinctLuminanceCount($slotA, $skyX, $skyY, $skyW, $skyH)
    Assert-True ($skyDistinctLuminance -ge $skyDistinctLuminanceFloor) `
        ("the skybox renders a photographic scene rather than a flat fill (distinct luminance values " +
         "$skyDistinctLuminance over cols $skyX-$($skyX + $skyW), rows $skyY-$($skyY + $skyH); need >= $skyDistinctLuminanceFloor)")

    # The HUD is legible near-white text on an OPAQUE (0.04, 0.05, 0.08) panel, and
    # it takes both halves of that description to identify it.
    #
    # Brightness alone does not discriminate: the same 1%-at-luminance-200 floor was
    # measured at 0.0218 on a RED run with the showcase entirely absent, satisfied
    # then by the editor's Controls panel and satisfiable now by a bright patch of
    # skybox. So require the panel too: nearly every pixel of this rectangle must be
    # near-black AND a fraction of it must be near-white. Scene content can be dark
    # or bright, but it cannot be 80% below luminance 40 while still carrying 1%
    # above 200 - the panel is what makes both true of the same rectangle at once.
    $hudDark = [ShowcaseFrame]::DarkRatio($slotA, $hudX, $hudY, $hudW, $hudH, $hudPanelLuminance)
    $hudBright = [ShowcaseFrame]::BrightRatio($slotA, $hudX, $hudY, $hudW, $hudH, 200)
    Assert-True (($hudDark -ge $hudPanelDarkFloor) -and ($hudBright -ge $hudTextBrightFloor)) `
        ("README capture mode renders the showcase HUD panel and its text at client (24,24) " +
         "(dark ratio $([Math]::Round($hudDark,4)) need >= $hudPanelDarkFloor; " +
         "bright ratio $([Math]::Round($hudBright,4)) need >= $hudTextBrightFloor)")

    # The clip assignment rotates: slot i plays clip ((cycle * 4 + i) % 7), so the
    # four names the HUD prints must change when the set does and must not change
    # while a set is running. Both halves are asserted, because either alone is
    # satisfiable by an implementation that is simply wrong: a caption that never
    # changed would pass the stability check, and one that flickered on capture
    # noise would pass the difference check.
    $frameCycle0 = $cycleFrames['cycle0-06']
    $frameCycle1 = $cycleFrames['cycle1-18']
    $cycle0Hud = [ShowcaseFrame]::Signature($frameCycle0, $hudX, $lineUpY, $hudW, $lineUpH)
    $cycle1Hud = [ShowcaseFrame]::Signature($frameCycle1, $hudX, $lineUpY, $hudW, $lineUpH)
    Assert-True ($cycle0Hud -ne $cycle1Hud) `
        'the clip assignment rotates between cycles (HUD names differ at t=6s and t=18s)'

    $withinCycle0 = @('cycle0-04', 'cycle0-06', 'cycle0-08' | ForEach-Object {
            [ShowcaseFrame]::Signature($cycleFrames[$_], $hudX, $lineUpY, $hudW, $lineUpH) })
    $distinctWithinCycle0 = @($withinCycle0 | Select-Object -Unique).Count
    Assert-True ($distinctWithinCycle0 -eq 1) `
        "the clip line-up holds still inside one 12 s set (found $distinctWithinCycle0 distinct HUD signatures at t=4,6,8 s)"

    $hudSignatures = @($cycleFrames.Keys | ForEach-Object {
            [ShowcaseFrame]::Signature($cycleFrames[$_], $hudX, $lineUpY, $hudW, $lineUpH) })
    $distinctHudSignatures = @($hudSignatures | Select-Object -Unique).Count
    Assert-True ($distinctHudSignatures -ge 2) `
        "at least two distinct clip line-ups appear over 24s (found $distinctHudSignatures)"

    # The caption has to name clips, not merely differ. Every sampled line-up must
    # carry as much ink as four "VRM_n" tokens do, so an implementation that blanked
    # the second HUD line - or printed one name and three empty slots - cannot pass
    # the two assertions above by removing text instead of rotating it.
    $lineUpInk = @($cycleFrames.Keys | ForEach-Object {
            [ShowcaseFrame]::BrightRatio($cycleFrames[$_], $hudX, $lineUpY, $hudW, $lineUpH, 200) })
    $minLineUpInk = ($lineUpInk | Measure-Object -Minimum).Minimum
    Assert-True ($minLineUpInk -ge 0.02) `
        ("every sampled HUD line-up rasterises four clip names (min bright ratio " +
         "$([Math]::Round($minLineUpInk,4)) over the line-up row, need >= 0.02)")

    # Separated moving column clusters prove that four characters are live rather
    # than one animating while the rest are frozen - and, because a character
    # hidden inside another's silhouette cannot contribute its own cluster, that
    # all four are separated.
    $frames = @($slotA, $slotB, $driftA, $driftB)
    $clusters = [ShowcaseFrame]::MovingColumnClusters($frames, $bandTop, $bandBottom,
        $bandColumnWidth, $bandTolerance, $bandMovingFraction, $bandMinGapColumns)
    Assert-True ($clusters -ge 4) `
        "four separated character regions animate independently (found $clusters, need >= 4)"

    # Same four characters, now asked what they are each doing. Every band is
    # profiled across the SAME half second - slot-a to slot-b - so the only thing
    # that can separate two bands is the motion itself, and the profiles are
    # normalised so a nearer character's larger sweep cannot pass for a different
    # clip. Four slots fed one clip in lock-step would produce one shape and score
    # 0; four different clips score all six pairs.
    $slotBands = @()
    $slotBandRanges = [ShowcaseFrame]::MovingColumnRanges($frames, $bandTop, $bandBottom,
        $bandColumnWidth, $bandTolerance, $bandMovingFraction, $bandMinGapColumns)
    for ($rangeIndex = 0; $rangeIndex -lt $slotBandRanges.Length; $rangeIndex += 2) {
        $slotBands += [pscustomobject]@{
            X     = $slotBandRanges[$rangeIndex]
            Width = $slotBandRanges[$rangeIndex + 1] - $slotBandRanges[$rangeIndex]
        }
    }
    # Widest four, left to right: if the scene ever produced a spurious fifth
    # cluster, the four character envelopes are the substantial ones.
    $slotBands = @($slotBands | Sort-Object -Property Width -Descending | Select-Object -First 4 | Sort-Object -Property X)

    $blueAccentCounts = @()
    $coolShadowRatios = @()
    $warmLightSamples = @()
    $strongPurpleSamples = @()
    $toonPaletteCandidateCounts = @()
    if ($slotBands.Count -eq 4) {
        $toonShoeHeight = $toonShoeBottom - $toonShoeTop
        $toonCostumeHeight = $toonCostumeBottom - $toonCostumeTop
        $toonStyleSamples = @(
            @{ Frame = $slotA; Reference = $slotB },
            @{ Frame = $cycleFrames['cycle0-06']; Reference = $cycleFrames['cycle0-04'] },
            @{ Frame = $cycleFrames['cycle0-08']; Reference = $cycleFrames['cycle0-095'] },
            @{ Frame = $cycleFrames['cycle1-16']; Reference = $cycleFrames['cycle1-14'] }
        )
        foreach ($slotBand in $slotBands) {
            $blueAccentCounts += [ShowcaseFrame]::BlueAccentCount(
                $slotA, $slotBand.X, $toonShoeTop, $slotBand.Width, $toonShoeHeight)
            $slotCoolSamples = @()
            foreach ($sample in $toonStyleSamples) {
                $ratios = @([ShowcaseFrame]::MovingToonPaletteRatios(
                    $sample.Frame, $sample.Reference, $slotBand.X, $toonCostumeTop,
                    $slotBand.Width, $toonCostumeHeight, $toonMotionColorTolerance))
                $slotCoolSamples += $ratios[0]
                $warmLightSamples += $ratios[1]
                $strongPurpleSamples += $ratios[2]
                $toonPaletteCandidateCounts += [int]$ratios[3]
            }
            $coolShadowRatios += ($slotCoolSamples | Measure-Object -Maximum).Maximum
        }
    }
    $minBlueAccent = if ($blueAccentCounts.Count -eq 4) { ($blueAccentCounts | Measure-Object -Minimum).Minimum } else { 0 }
    $minCoolShadow = if ($coolShadowRatios.Count -eq 4) { ($coolShadowRatios | Measure-Object -Minimum).Minimum } else { 0.0 }
    $minWarmLight = if ($warmLightSamples.Count -eq 16) { ($warmLightSamples | Measure-Object -Minimum).Minimum } else { 0.0 }
    $maxStrongPurple = if ($strongPurpleSamples.Count -eq 16) { ($strongPurpleSamples | Measure-Object -Maximum).Maximum } else { 1.0 }
    $minPaletteCandidates = if ($toonPaletteCandidateCounts.Count -eq 16) {
        ($toonPaletteCandidateCounts | Measure-Object -Minimum).Minimum
    } else { 0 }
    Assert-True (($slotBands.Count -eq 4) -and ($minBlueAccent -ge $toonBlueAccentMinimum)) `
        ("all four rendered characters use SampleModel's navy shoe accent (counts " +
         "$(($blueAccentCounts | ForEach-Object { $_ }) -join '/') pixels; each needs >= $toonBlueAccentMinimum)")
    Assert-True (($slotBands.Count -eq 4) -and ($minCoolShadow -ge $toonCoolShadowMinimum) -and
        ($minWarmLight -ge $toonWarmLightMinimum) -and ($maxStrongPurple -le $toonStrongPurpleMaximum) -and
        ($minPaletteCandidates -ge $toonPaletteCandidateMinimum)) `
        ("toon shadows stay cool and balanced across all four moving costumes (per-slot cool peaks " +
         "$(($coolShadowRatios | ForEach-Object { [Math]::Round($_, 3) }) -join '/'); " +
         "min warm $([Math]::Round($minWarmLight, 3)) >= $toonWarmLightMinimum; " +
         "max strong-purple $([Math]::Round($maxStrongPurple, 3)) <= $toonStrongPurpleMaximum; " +
         "min candidates $minPaletteCandidates >= $toonPaletteCandidateMinimum)")

    $slotPairsDiffering = 0
    $slotPairReport = @()
    if ($slotBands.Count -eq 4) {
        $slotProfiles = New-Object 'System.Collections.Generic.List[double[]]'
        foreach ($slotBand in $slotBands) {
            $slotProfiles.Add([ShowcaseFrame]::MotionProfile($slotA, $slotB,
                    $slotBand.X, $bandTop, $slotBand.Width, ($bandBottom - $bandTop),
                    $slotMotionCellsX, $slotMotionCellsY, $bandTolerance))
        }
        for ($left = 0; $left -lt $slotBands.Count; ++$left) {
            for ($right = $left + 1; $right -lt $slotBands.Count; ++$right) {
                $shapeDistance = [ShowcaseFrame]::MotionShapeDistance($slotProfiles[$left], $slotProfiles[$right])
                $slotPairReport += ('{0}-{1}:{2}' -f $left, $right, [Math]::Round($shapeDistance, 3))
                if ($shapeDistance -ge $slotMotionShapeThreshold) { ++$slotPairsDiffering }
            }
        }
    }
    else {
        $slotPairReport += "only $($slotBands.Count) character bands were separable"
    }

    Assert-True ($slotPairsDiffering -eq 6) `
        "all four characters show different motion (differing slot pairs $slotPairsDiffering of 6; over $slotSampleGapSec s, shape distances $($slotPairReport -join ' ') vs threshold $slotMotionShapeThreshold)"

    # Cast height band. Computed the same way MovingColumnRanges finds a
    # character's horizontal envelope, but over rows and without clustering: the
    # cast's vertical extent is one bounding box (head of the nearest character to
    # feet of the farthest), not four separated bands, so MovingRowExtent walks
    # every row bin across the full frame width and marks it moving at the same
    # pixel tolerance ($bandTolerance) the column scan uses, then returns the
    # first and last row that qualified. Measured over the same slot-a/slot-b
    # half second the motion assertions above use. $castRowHeight and
    # $castRowMovingFraction are declared beside $castHeightMinFraction /
    # $castHeightMaxFraction above - see the comment there for why each needs its
    # own constant instead of reusing the column scan's.
    $castExtent = [ShowcaseFrame]::MovingRowExtent($slotA, $slotB, 0, $clientWidth,
        $castRowHeight, $bandTolerance, $castRowMovingFraction)
    $castHeightFraction = if ($castExtent.Length -eq 2) { ($castExtent[1] - $castExtent[0]) / [double]$clientHeight } else { 0.0 }
    $castRowsText = if ($castExtent.Length -eq 2) { "rows $($castExtent[0])-$($castExtent[1]) of $clientHeight" } else { 'no moving rows found' }
    Assert-True (($castExtent.Length -eq 2) -and ($castHeightFraction -ge $castHeightMinFraction) -and ($castHeightFraction -le $castHeightMaxFraction)) `
        ("the cast occupies a $castHeightMinFraction-$castHeightMaxFraction fraction of frame height across the " +
         "${slotSampleGapSec}s slot-a/slot-b sample (measured $([Math]::Round($castHeightFraction,4)); $castRowsText)")

    # No body at a frame edge. The relaxed requirement the human ruled on: finger
    # tips and cloth/spring bones may brush an edge briefly; a torso, limb, or
    # head may not, ever. The discriminator is run length - a body crossing an
    # edge changes a large run of that edge line across this half second, while a
    # fingertip or sleeve tip changes only a handful of pixels along it - so each
    # of the four edge lines (both full-height side columns and both full-width
    # top/bottom rows) is measured with the same DiffRatio/tolerance the rest of
    # this file uses, and every edge must stay under the threshold.
    #
    # Each edge is read as the MINIMUM DiffRatio across $edgeBandDepth successive
    # single-pixel depths stepping inward from the physical edge, not the
    # outermost line alone - see $edgeBandDepth above for why: that is what keeps
    # this guard from firing on a capture rectangle that landed one pixel off
    # while staying just as sensitive to an actual body.
    #
    # Measured over every consecutive pair the show already gave us a frame for,
    # not just the slot-a/slot-b half second at t=0.6-1.1s: that window is barely
    # a second into a 24 s show, and the $cycleFrames captured above for the
    # HUD-signature checks (t=4/6/8s in cycle 0, t=14/16/18/22s in cycle 1) supply
    # five more consecutive pairs spanning both 12 s sets - including every slot
    # in cycle 1, which the slot-a/slot-b window alone never reaches - at zero
    # extra runtime. Each pair contributes its own four edge fractions and
    # $maxEdgeFraction takes the maximum per edge across all of them, so a body
    # that only grazes an edge at, say, t=14-16s cannot be missed by a guard that
    # only ever looked at t=0.6-1.1s.
    $edgeFramePairs = @(
        , @($slotA, $slotB)
        , @($cycleFrames['cycle0-04'], $cycleFrames['cycle0-06'])
        , @($cycleFrames['cycle0-06'], $cycleFrames['cycle0-08'])
        , @($cycleFrames['cycle1-14'], $cycleFrames['cycle1-16'])
        , @($cycleFrames['cycle1-16'], $cycleFrames['cycle1-18'])
        , @($cycleFrames['cycle1-18'], $cycleFrames['cycle1-22'])
    )
    $edgeFractions = [ordered]@{ left = 0.0; right = 0.0; top = 0.0; bottom = 0.0 }
    $edgePairReports = @()
    foreach ($edgePair in $edgeFramePairs) {
        $pairA = $edgePair[0]
        $pairB = $edgePair[1]
        # $edgeBandDepth successive single-pixel lines stepping inward from each
        # physical edge; the MINIMUM across them is the per-edge reading below (see
        # $edgeBandDepth above), so a one-pixel capture offset - which only ever
        # disturbs depth 0 - cannot pass for a body, which disturbs every depth.
        $leftDepths = @(0..($edgeBandDepth - 1) | ForEach-Object {
                [ShowcaseFrame]::DiffRatio($pairA, $pairB, $_, 0, 1, $clientHeight, $bandTolerance) })
        $rightDepths = @(0..($edgeBandDepth - 1) | ForEach-Object {
                [ShowcaseFrame]::DiffRatio($pairA, $pairB, ($clientWidth - 1 - $_), 0, 1, $clientHeight, $bandTolerance) })
        $topDepths = @(0..($edgeBandDepth - 1) | ForEach-Object {
                [ShowcaseFrame]::DiffRatio($pairA, $pairB, 0, $_, $clientWidth, 1, $bandTolerance) })
        $bottomDepths = @(0..($edgeBandDepth - 1) | ForEach-Object {
                [ShowcaseFrame]::DiffRatio($pairA, $pairB, 0, ($clientHeight - 1 - $_), $clientWidth, 1, $bandTolerance) })
        $pairEdges = [ordered]@{
            left   = ($leftDepths | Measure-Object -Minimum).Minimum
            right  = ($rightDepths | Measure-Object -Minimum).Minimum
            top    = ($topDepths | Measure-Object -Minimum).Minimum
            bottom = ($bottomDepths | Measure-Object -Minimum).Minimum
        }
        foreach ($edgeKey in $pairEdges.Keys) {
            if ($pairEdges[$edgeKey] -gt $edgeFractions[$edgeKey]) { $edgeFractions[$edgeKey] = $pairEdges[$edgeKey] }
        }
        $edgePairReports += ($pairEdges.Keys | ForEach-Object { "$($_)=$([Math]::Round($pairEdges[$_],4))" }) -join ','
    }
    $maxEdgeFraction = ($edgeFractions.Values | Measure-Object -Maximum).Maximum
    $edgeReport = ($edgeFractions.Keys | ForEach-Object { "$($_)=$([Math]::Round($edgeFractions[$_],4))" }) -join ', '
    Assert-True ($maxEdgeFraction -lt $edgeBodyThreshold) `
        ("no body crosses a frame edge across any sampled pair (max edge changed-pixel fractions $edgeReport; " +
         "all need < $edgeBodyThreshold; per-pair $($edgePairReports -join ' | '))")

    # -----------------------------------------------------------------------
    # Technique windows. Each 12 s set runs a cross-fade at 0.0-0.6 s and an
    # upper-body layer on slot 0 at 4.0-7.0 s. The two windows do not overlap, so
    # each of the two measurements below reads one technique with the other off.
    # -----------------------------------------------------------------------

    # Locate the set change inside the boundary burst by the one thing that marks
    # it unambiguously: the HUD's clip line-up, which changes at a set boundary and
    # nowhere else. The pair it changes across IS the straddling pair, so the
    # measurement cannot be defeated by a residual offset between this stopwatch
    # and the show's clock - and cannot pass vacuously on two ordinary frames,
    # because a burst that never crossed the boundary yields no such pair at all
    # and fails here instead.
    $stepRegionHeight = $clientHeight - $stepRegionTop
    $boundarySignatures = @($boundaryFrames | ForEach-Object {
            [ShowcaseFrame]::Signature($_, $hudX, $lineUpY, $hudW, $lineUpH) })
    $boundaryPairIndex = -1
    for ($sigIndex = 0; $sigIndex + 1 -lt $boundarySignatures.Count; ++$sigIndex) {
        if ($boundarySignatures[$sigIndex] -ne $boundarySignatures[$sigIndex + 1]) {
            $boundaryPairIndex = $sigIndex
            break
        }
    }
    Assert-True ($boundaryPairIndex -ge 0) `
        ("the ${boundaryBurstCount}-frame burst from t=${boundaryBurstStart}s spans the first set change " +
         "(a consecutive pair changes the clip line-up), so the cross-fade measurement below is not vacuous")

    $stepValues = @()
    for ($stepIndex = 0; $stepIndex + 1 -lt $stepFrames.Count; ++$stepIndex) {
        $stepValues += [ShowcaseFrame]::MeanAbsDiff($stepFrames[$stepIndex], $stepFrames[$stepIndex + 1],
            0, $stepRegionTop, $clientWidth, $stepRegionHeight)
    }
    $sortedSteps = @($stepValues | Sort-Object)
    $medianStep = if ($sortedSteps.Count -gt 0) { [double]$sortedSteps[[int][Math]::Floor($sortedSteps.Count / 2)] } else { 0.0 }
    $boundarySteps = @()
    for ($stepIndex = 0; $stepIndex + 1 -lt $boundaryFrames.Count; ++$stepIndex) {
        $boundarySteps += [ShowcaseFrame]::MeanAbsDiff($boundaryFrames[$stepIndex], $boundaryFrames[$stepIndex + 1],
            0, $stepRegionTop, $clientWidth, $stepRegionHeight)
    }
    $boundaryStep = if ($boundaryPairIndex -ge 0) { [double]$boundarySteps[$boundaryPairIndex] } else { [double]::MaxValue }
    Write-Host ("  ..   boundary burst steps " + (($boundarySteps | ForEach-Object { [Math]::Round($_, 3) }) -join ' ') +
        " (set change across pair $boundaryPairIndex); mid-set steps " +
        (($stepValues | ForEach-Object { [Math]::Round($_, 3) }) -join ' '))

    # A hard clip switch replaces every character's pose between one frame and the
    # next, so the whole difference between two unrelated poses lands in the single
    # pair that contains the boundary - many times an ordinary 0.2 s step. A 0.6 s
    # cross-fade spreads it: at t = 12.1 s only smoothstep(0.1/0.6) = 0.074 of the
    # incoming line-up has arrived, so the boundary pair stays in the same league
    # as a mid-set one. Catches a regression that drops the blend, mistimes its
    # window, or breaks the outgoing clip's continuity at the seam.
    Assert-True ($boundaryStep -le ($medianStep * 2.0)) `
        ("the set boundary cross-fades rather than snapping (boundary step $([Math]::Round($boundaryStep,2)) vs median $([Math]::Round($medianStep,2)))")

    # Negative-cycle coverage for PortfolioClipIndexForSlot. Throughout cycle 0 the
    # per-slot loop looks the outgoing line-up up with cycle - 1 == -1 on every slot
    # of every frame. C++ '%' truncates toward zero, so the raw remainder there is
    # -4..-1; only that function's fold-back turns it into a real index, and without
    # it showcase.clips[(size_t)index] would read a 7-element vector at ~2^64 and
    # take the Debug build down with it. This is that path's first automated caller:
    # it requires the whole of cycle 0 to have been rendered by a process that was
    # still alive and still drawing its HUD at t = 11.5 s - the last still before
    # the first set change - with four characters separable throughout.
    $showcaseProcess.Refresh()
    $cycle0EndDark = [ShowcaseFrame]::DarkRatio($cycleFrames['boundary-a'], $hudX, $hudY, $hudW, $hudH, $hudPanelLuminance)
    $cycle0EndBright = [ShowcaseFrame]::BrightRatio($cycleFrames['boundary-a'], $hudX, $hudY, $hudW, $hudH, 200)
    Assert-True ((-not $showcaseProcess.HasExited) -and ($clusters -ge 4) -and
        ($cycle0EndDark -ge $hudPanelDarkFloor) -and ($cycle0EndBright -ge $hudTextBrightFloor)) `
        ("cycle 0 renders through its negative (cycle - 1 = -1) outgoing-clip lookup on every slot of every " +
         "frame (alive at t=11.5s, $clusters separable characters, HUD dark " +
         "$([Math]::Round($cycle0EndDark,4)) / bright $([Math]::Round($cycle0EndBright,4)))")

    # Upper-body layer on slot 0. Screen order left to right is slot 1, slot 2,
    # slot 0, slot 3 (see kPortfolioScreenOrderToSlot), so the third of the four
    # separated envelopes measured above is slot 0's - derived from the frames
    # rather than hard-coded, exactly as the motion-variety assertion derives them.
    $slot0Band = if ($slotBands.Count -eq 4) { $slotBands[2] } else { $null }
    $layerControlRatio = 0.0
    $layerWindowPairCount = 0
    $layerCaptionSeparates = $false
    $layerReport = ''
    if ($null -eq $slot0Band) {
        $layerReport = "only $($slotBands.Count) character bands were separable, so slot 0's band is unknown"
    }
    else {
        # Slot 0's arm band against the SAME rows on the three companions. The
        # companions are screen positions 0, 1 and 3 - slots 1, 2 and 3, none of
        # which is ever handed desc.upper - and they are read out of the same two
        # frames, so every lighting, exposure and capture property the four
        # characters share divides out. Their mean, not one of them: a single
        # companion whose clip happens to be still for half a second would other-
        # wise decide the answer on its own.
        $layerControlSlots = @(0, 1, 3)
        $layerRatio = {
            param([ShowcaseFrame]$A, [ShowcaseFrame]$B)

            $bandHeight = $layerUpperBottom - $layerUpperTop
            $main = [ShowcaseFrame]::MeanAbsDiff($A, $B, $slot0Band.X, $layerUpperTop, $slot0Band.Width, $bandHeight)
            $controls = @($layerControlSlots | ForEach-Object {
                    [ShowcaseFrame]::MeanAbsDiff($A, $B, $slotBands[$_].X, $layerUpperTop,
                        $slotBands[$_].Width, $bandHeight) })
            $control = ($controls | Measure-Object -Average).Average
            if ($control -le 0.0001) { return 0.0 }
            return $main / $control
        }

        # Which of the swept frames the show itself says the layer is running on.
        # Self-calibrating: the sweep straddles the window, so its technique row
        # carries exactly two ink values, and the layer's caption is the longer of
        # the two strings - see $techniqueY. The stopwatch picks WHEN to capture;
        # the HUD picks which captures count, so a residual offset between this
        # clock and the show's slides the sampled instants without ever sampling
        # the wrong side of the window boundary.
        $layerCaptionInk = [ordered]@{}
        foreach ($sweepName in $layerSweepNames) {
            $layerCaptionInk[$sweepName] = [ShowcaseFrame]::BrightRatio($cycleFrames[$sweepName],
                $hudX, $techniqueY, $hudW, $techniqueH, 200)
        }
        $minCaptionInk = ($layerCaptionInk.Values | Measure-Object -Minimum).Minimum
        $maxCaptionInk = ($layerCaptionInk.Values | Measure-Object -Maximum).Maximum
        $layerCaptionSeparates = ($minCaptionInk -gt 0.0) -and
            ($maxCaptionInk -ge ($minCaptionInk * $techniqueCaptionInkRatio))
        $captionCut = ($minCaptionInk + $maxCaptionInk) / 2.0

        $layerWindowRatios = @()
        $layerBaseRatios = @()
        for ($sweepIndex = 0; $sweepIndex + 1 -lt $layerSweepNames.Count; ++$sweepIndex) {
            $firstIn = $layerCaptionInk[$layerSweepNames[$sweepIndex]] -ge $captionCut
            $secondIn = $layerCaptionInk[$layerSweepNames[$sweepIndex + 1]] -ge $captionCut
            $pairRatio = & $layerRatio $cycleFrames[$layerSweepNames[$sweepIndex]] `
                $cycleFrames[$layerSweepNames[$sweepIndex + 1]]
            # Pairs that straddle the boundary belong to neither side and are
            # dropped: half of that half second carries the layer and half does not.
            if ($firstIn -and $secondIn) { $layerWindowRatios += $pairRatio }
            elseif ((-not $firstIn) -and (-not $secondIn)) { $layerBaseRatios += $pairRatio }
        }
        $layerWindowPairCount = $layerWindowRatios.Count
        if ($layerWindowPairCount -gt 0) {
            $sortedLayerRatios = @($layerWindowRatios | Sort-Object)
            $layerControlRatio = [double]$sortedLayerRatios[[int][Math]::Floor($sortedLayerRatios.Count / 4)]
        }
        # Reported, not asserted on: the same ratio over the swept pairs the HUD
        # calls BASE. It reads 0.10-0.22 whether or not the layer exists - the
        # window is the only place slot 0's upper body is asked to do anything the
        # companions are not - which is what makes the in-window reading a claim
        # about the layer rather than about slot 0 being a busier character.
        $layerBaseReport = if ($layerBaseRatios.Count -gt 0) {
            (($layerBaseRatios | ForEach-Object { [Math]::Round($_, 3) }) -join '/')
        }
        else { 'none sampled' }
        $layerReport = ("slot 0 band x $($slot0Band.X)-$($slot0Band.X + $slot0Band.Width); " +
            "$layerWindowPairCount in-window pairs, per-pair " +
            (($layerWindowRatios | ForEach-Object { [Math]::Round($_, 2) }) -join ' ') +
            "; caption ink $([Math]::Round($minCaptionInk,4))-$([Math]::Round($maxCaptionInk,4)); " +
            "out-of-window pairs $layerBaseReport")
    }

    # With the layer on, slot 0's upper body is driven by a second clip through an
    # alpha that only reaches zero at the two ends of the window, so it never goes
    # quiet while the window is open: on a typical sampled half second its arm band
    # changes about as much as the companions' do, and on its quietest one still
    # about three quarters as much. With desc.upper dropped, layered onto the wrong
    # slot, or left at layerAlpha = 0, slot 0 falls back to VRM_5's own near-still
    # 4-7 s and the quartile collapses to ~0.12 - measured on a scratch build that
    # changed nothing but `desc.upper.enabled`. See $layerControlRatioFloor above
    # for the six runs behind those numbers and for why the floor sits where it does.
    Assert-True ($layerCaptionSeparates -and ($layerWindowPairCount -ge $layerMinPairs) -and
        ($layerControlRatio -ge $layerControlRatioFloor)) `
        ("slot 0's upper body keeps pace with the cast only while the layer runs (lower-quartile ratio " +
         "$([Math]::Round($layerControlRatio,4)) vs floor $layerControlRatioFloor; measured 0.11-0.13 with the layer " +
         "removed; need >= $layerMinPairs in-window pairs and a technique caption that changes across the sweep, " +
         "which was $layerCaptionSeparates); $layerReport")

    # There is no third technique assertion. A CCD IK window ran on slot 0's left
    # hand at set time 8.0-11.4 and was measured here as the yellow reach line its
    # debug pass drew; both the window and the debug pass are gone from the runtime,
    # so the assertion went with them rather than being retimed onto a window that no
    # longer exists. Nothing replaces it, and nothing needs to: set time 7.0-12.0 now
    # runs no named technique, so there is no claim about it left to make. The three
    # samples still taken inside that stretch - cycle0-075, cycle0-08 and cycle0-095 -
    # are read by the HUD line-up assertions, which walk every sampled frame, and
    # cycle0-06/cycle0-08 is one of the pairs the frame-edge guard measures.

    Reset-ShowcaseTopmost -Handle $showcaseHandle
    Stop-ShowcaseProcess -Process $showcaseProcess

    # -----------------------------------------------------------------------
    # 4. Interactive launch: a reader who double-clicks Project 36 gets the
    #    original editor surfaces. This run also supplies a writable backbuffer
    #    path while capture mode is off, proving publication remains opt-in.
    # -----------------------------------------------------------------------
    Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue
    $env:DX11_README_BACKBUFFER_PNG = $plainBackbufferPath
    $interactiveProcess = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
    if ($null -eq $previousCaptureEnv) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $previousCaptureEnv }
    if ($null -eq $previousBackbufferEnv) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue }
    else { $env:DX11_README_BACKBUFFER_PNG = $previousBackbufferEnv }

    $interactiveWindow = Wait-ShowcaseWindow -Process $interactiveProcess
    $interactiveHandle = $interactiveWindow.Handle
    Set-ShowcaseClientSize -Handle $interactiveHandle -Width $clientWidth -Height $clientHeight
    Set-ShowcaseTopmost -Handle $interactiveHandle
    Start-Sleep -Milliseconds 400
    Wait-ShowcaseScene -Process $interactiveProcess -Handle $interactiveHandle -Width $clientWidth -Height $clientHeight `
        -ProbePath (Join-Path $frameDir 'interactive-probe.png') -ClickToStart
    Start-Sleep -Milliseconds 500
    $interactiveFrame = Save-ShowcaseFrame -Handle $interactiveHandle -Width $clientWidth -Height $clientHeight `
        -Path (Join-Path $frameDir 'interactive-panels.png')

    # ImGui receives logical 1066x600 coordinates on the 150%-scaled test desktop while the
    # Direct3D client is captured at 1600x900. These rectangles follow the actual ImGui display
    # coordinates; they intentionally do not multiply by the OS scale factor.
    $shadowPanelDark = [ShowcaseFrame]::DarkRatio($interactiveFrame, 10, 390, 370, 210, 45)
    $consolePanelDark = [ShowcaseFrame]::DarkRatio($interactiveFrame, 120, 390, 830, 200, 45)
    $scenePanelDark = [ShowcaseFrame]::DarkRatio($interactiveFrame, 736, 20, 320, 360, 45)
    $advancedPanelDark = [ShowcaseFrame]::DarkRatio($interactiveFrame, 400, 20, 390, 150, 45)
    $deferredPanelDark = [ShowcaseFrame]::DarkRatio($interactiveFrame, 400, 180, 260, 105, 45)
    Assert-True ($shadowPanelDark -ge 0.35) `
        ("ordinary launch restores the ShadowMap settings panel (dark coverage $([Math]::Round($shadowPanelDark,3)), need >= 0.35)")
    Assert-True ($consolePanelDark -ge 0.45) `
        ("ordinary launch restores the Console panel (dark coverage $([Math]::Round($consolePanelDark,3)), need >= 0.45)")
    Assert-True ($scenePanelDark -ge 0.45) `
        ("ordinary launch restores the Scene Collection / Details panels (dark coverage $([Math]::Round($scenePanelDark,3)), need >= 0.45)")
    Assert-True ($advancedPanelDark -ge 0.45) `
        ("ordinary launch shows the Advanced Rig showcase-mode explanation (dark coverage $([Math]::Round($advancedPanelDark,3)), need >= 0.45)")
    Assert-True ($deferredPanelDark -ge 0.45) `
        ("ordinary launch shows the Deferred forward-only explanation (dark coverage $([Math]::Round($deferredPanelDark,3)), need >= 0.45)")
    Assert-True (@(Get-ChildItem -LiteralPath $plainBackbufferDir -File -ErrorAction SilentlyContinue).Count -eq 0) `
        'with README capture mode off the backbuffer writer stays silent even though DX11_README_BACKBUFFER_PNG names a writable path'

    Reset-ShowcaseTopmost -Handle $interactiveHandle
    Stop-ShowcaseProcess -Process $interactiveProcess

    # -----------------------------------------------------------------------
    # 5. Opt-in backbuffer publication, which needs README capture mode AND
    #    DX11_README_BACKBUFFER_PNG, so it gets its own run with both set. The
    #    capture tool must be able to take the true rendered frame out of the swap
    #    chain instead of screen-scraping the window, so the running application
    #    has to keep a complete 1600x900 PNG at exactly the path the variable
    #    named.
    #
    #    The polling loop below observes the file while the application keeps
    #    rewriting it. Publication through a temporary sibling plus an atomic
    #    replace is the only way every one of those observations can be either
    #    "absent" or "fully decodable": a writer that encoded straight into the
    #    published path would be caught mid-write as a truncated or unreadable
    #    file, and the file is rewritten ~12 times a second, so a torn write has
    #    dozens of chances to be seen.
    # -----------------------------------------------------------------------
    $env:DX11_README_CAPTURE = '1'
    $env:DX11_README_BACKBUFFER_PNG = $backbufferPath
    $captureProcess = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
    if ($null -eq $previousCaptureEnv) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $previousCaptureEnv }
    if ($null -eq $previousBackbufferEnv) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue }
    else { $env:DX11_README_BACKBUFFER_PNG = $previousBackbufferEnv }

    $captureWindow = Wait-ShowcaseWindow -Process $captureProcess
    $captureHandle = $captureWindow.Handle
    Set-ShowcaseClientSize -Handle $captureHandle -Width $clientWidth -Height $clientHeight
    Set-ShowcaseTopmost -Handle $captureHandle
    Start-Sleep -Milliseconds 400
    Wait-ShowcaseScene -Process $captureProcess -Handle $captureHandle -Width $clientWidth -Height $clientHeight `
        -ProbePath (Join-Path $frameDir 'capture-probe.png')

    $captureProcess.Refresh()
    Assert-True (-not $captureProcess.HasExited) 'capture-mode process reaches the live scene without a click'

    $backbufferObservations = 0
    $backbufferComplete = 0
    $backbufferAbsent = 0
    $backbufferContended = 0
    $backbufferTornCount = 0
    $backbufferAnomalies = [System.Collections.Generic.List[string]]::new()
    $backbufferSizes = [System.Collections.Generic.HashSet[string]]::new()
    $backbufferSignatures = [System.Collections.Generic.List[string]]::new()
    $nextSignatureAt = [datetime]::MinValue
    $pollDeadline = (Get-Date).AddSeconds(6)
    while ((Get-Date) -lt $pollDeadline) {
        $observation = Read-ShowcaseBackbufferPng -Path $backbufferPath
        $backbufferObservations++
        if ($observation.Contended) {
            # The open lost a race with the publisher's atomic rename
            # (ERROR_SHARING_VIOLATION / ERROR_LOCK_VIOLATION): the reader observed
            # nothing at all, which is not evidence against atomicity.
            $backbufferContended++
        }
        elseif (-not $observation.Observed) {
            $backbufferAbsent++
        }
        elseif ($observation.Complete) {
            $backbufferComplete++
            [void]$backbufferSizes.Add("$($observation.Width)x$($observation.Height)")
            # Sampled far apart relative to the 12 fps publication throttle, so
            # two identical samples would mean a stale, never-refreshed file.
            if ((Get-Date) -ge $nextSignatureAt) {
                $backbufferSignatures.Add($observation.Signature)
                $nextSignatureAt = (Get-Date).AddMilliseconds(500)
            }
        }
        else {
            $backbufferTornCount++
            if ($backbufferAnomalies.Count -lt 8) {
                $backbufferAnomalies.Add($observation.Reason)
            }
        }
        Start-Sleep -Milliseconds 25
    }

    Assert-True ($backbufferComplete -ge 10) `
        ("DX11_README_BACKBUFFER_PNG publishes to exactly the requested path (decoded $backbufferComplete of $backbufferObservations observations, need >= 10)")
    Assert-True ($backbufferSizes.Count -eq 1 -and $backbufferSizes.Contains("${clientWidth}x${clientHeight}")) `
        ("every published backbuffer frame decodes at exactly ${clientWidth}x${clientHeight} (saw: $(($backbufferSizes | Sort-Object) -join ', '))")
    Assert-True ($backbufferTornCount -eq 0) `
        ("publication is atomic: no observation ever caught a partially written PNG (complete=$backbufferComplete, absent=$backbufferAbsent, contended=$backbufferContended, torn=$backbufferTornCount, total=$backbufferObservations; torn reasons: $($backbufferAnomalies -join ' | '))")
    $distinctSignatures = @($backbufferSignatures | Select-Object -Unique).Count
    Assert-True ($distinctSignatures -ge 2) `
        ("the published PNG refreshes with live frames rather than one stale frame ($distinctSignatures distinct of $($backbufferSignatures.Count) half-second samples, need >= 2)")

    Reset-ShowcaseTopmost -Handle $captureHandle
    $captureClosedCleanly = Close-ShowcaseProcess -Process $captureProcess -Handle $captureHandle

    # The temporary sibling is an implementation detail of the atomic replace and
    # must never survive the run; a leaked one would end up in the media directory
    # the capture tool hands over.
    Assert-True (-not (Test-Path -LiteralPath $backbufferTempPath)) `
        ("the atomic publication temporary leaves nothing behind after the process exits (clean shutdown: $captureClosedCleanly)")

    # -----------------------------------------------------------------------
    # 6. The other half of the opt-in gate: capture mode on, no
    #    DX11_README_BACKBUFFER_PNG. This is how the other 36 projects run, and
    #    the writer must publish nothing at all. With no path named, the only
    #    place a publication could land is beside the executable under some
    #    implied default name, so that is what the before/after snapshot of the
    #    runtime directory below measures.
    # -----------------------------------------------------------------------
    $runtimePngBefore = @(Get-ChildItem -LiteralPath $runtimeDir -Filter '*.png' -File -ErrorAction SilentlyContinue |
        ForEach-Object { $_.Name })

    Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue
    $env:DX11_README_CAPTURE = '1'
    $gateProcess = Start-Process -FilePath $exePath -WorkingDirectory $runtimeDir -PassThru
    if ($null -eq $previousCaptureEnv) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $previousCaptureEnv }
    if ($null -eq $previousBackbufferEnv) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue }
    else { $env:DX11_README_BACKBUFFER_PNG = $previousBackbufferEnv }

    $gateWindow = Wait-ShowcaseWindow -Process $gateProcess
    $gateHandle = $gateWindow.Handle
    Set-ShowcaseClientSize -Handle $gateHandle -Width $clientWidth -Height $clientHeight
    Set-ShowcaseTopmost -Handle $gateHandle
    Start-Sleep -Milliseconds 400
    Wait-ShowcaseScene -Process $gateProcess -Handle $gateHandle -Width $clientWidth -Height $clientHeight `
        -ProbePath (Join-Path $frameDir 'gate-probe.png')
    # Long enough for two dozen throttled publications, had any been due.
    Start-Sleep -Milliseconds 2500

    $gateProcess.Refresh()
    Assert-True (-not $gateProcess.HasExited) 'capture mode without DX11_README_BACKBUFFER_PNG keeps running normally'

    Reset-ShowcaseTopmost -Handle $gateHandle
    Stop-ShowcaseProcess -Process $gateProcess

    $newRuntimePngs = @(Get-ChildItem -LiteralPath $runtimeDir -Filter '*.png' -File -ErrorAction SilentlyContinue |
        Where-Object { $runtimePngBefore -notcontains $_.Name } | ForEach-Object { $_.Name })
    Assert-True ($newRuntimePngs.Count -eq 0) `
        ("capture mode without DX11_README_BACKBUFFER_PNG writes no implied default PNG beside the executable (new files: $($newRuntimePngs -join ', '))")
}
finally {
    Reset-ShowcaseTopmost -Handle $showcaseHandle
    Reset-ShowcaseTopmost -Handle $interactiveHandle
    Reset-ShowcaseTopmost -Handle $captureHandle
    Reset-ShowcaseTopmost -Handle $gateHandle
    Stop-ShowcaseProcess -Process $showcaseProcess
    Stop-ShowcaseProcess -Process $interactiveProcess
    Stop-ShowcaseProcess -Process $captureProcess
    Stop-ShowcaseProcess -Process $gateProcess
    if ($null -eq $previousCaptureEnv) { Remove-Item Env:\DX11_README_CAPTURE -ErrorAction SilentlyContinue }
    else { $env:DX11_README_CAPTURE = $previousCaptureEnv }
    if ($null -eq $previousBackbufferEnv) { Remove-Item Env:\DX11_README_BACKBUFFER_PNG -ErrorAction SilentlyContinue }
    else { $env:DX11_README_BACKBUFFER_PNG = $previousBackbufferEnv }
    # Every process is down, so whatever ini they wrote is final. Discard it and
    # restore the developer's panel layout byte for byte.
    if (Test-Path -LiteralPath $imguiIniPath -PathType Leaf) {
        Remove-Item -LiteralPath $imguiIniPath -Force -ErrorAction SilentlyContinue
    }
    if ($imguiIniExisted -and $null -ne $imguiIniBytes) {
        [System.IO.File]::WriteAllBytes($imguiIniPath, $imguiIniBytes)
    }
    if (-not $KeepFrames -and (Test-Path -LiteralPath $frameDir)) {
        Remove-Item -LiteralPath $frameDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    elseif ($KeepFrames) {
        Write-Host "  frames kept in $frameDir"
    }
}

if ($script:Failures.Count -gt 0) {
    Write-Host 'Project 36 portfolio showcase verification failed:'
    foreach ($failure in $script:Failures) { Write-Host " - $failure" }
    exit 1
}

Write-Host 'Project 36 portfolio showcase verification passed.'
