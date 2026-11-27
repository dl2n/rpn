using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;

public static class RpnDisplayFixture
{
    public static bool HasErrorIndicator(string path)
    {
        using (var image = new Bitmap(path))
        {
            // The first status column is reserved for the green error marker.
            for (int y = 0; y < image.Height / 5; ++y)
                for (int x = 0; x < image.Width / 35; ++x)
                {
                    Color pixel = image.GetPixel(x, y);
                    if (pixel.G >= 32 && pixel.R == 0 && pixel.B == 0)
                        return true;
                }
            return false;
        }
    }

    public static void AssertSameRows(string beforePath, string afterPath)
    {
        using (var before = new Bitmap(beforePath))
        using (var after = new Bitmap(afterPath))
        {
            if (before.Size == after.Size && SameGlyphs(Glyphs(before), Glyphs(after)))
                return;
        }
        throw new InvalidOperationException("The five display rows changed: '" + beforePath +
            "' versus '" + afterPath + "'.");
    }

    private static byte[,] Glyphs(Bitmap image)
    {
        var glyphs = new byte[image.Height, image.Width];
        bool[] rows = new bool[5];
        for (int y = 0; y < image.Height; ++y)
        {
            int row = 5 * y / image.Height;
            for (int x = 0; x < image.Width; ++x)
            {
                Color pixel = image.GetPixel(x, y);
                if (pixel.R >= 32 && pixel.G == 0 && pixel.B == 0)
                    glyphs[y, x] = 1;
                else if (pixel.G >= 32 && pixel.R == 0 && pixel.B == 0)
                    glyphs[y, x] = 2;
                rows[row] |= glyphs[y, x] == (row == 0 ? 2 : 1);
            }
        }
        if (Array.IndexOf(rows, false) >= 0)
            throw new InvalidOperationException("The display crop lacks visible text in one of its five rows.");
        return glyphs;
    }

    private static bool SameGlyphs(byte[,] before, byte[,] after)
    {
        for (int y = 0; y < before.GetLength(0); ++y)
            for (int x = 0; x < before.GetLength(1); ++x)
                if (before[y, x] != after[y, x])
                    return false;
        return true;
    }

    public static void AssertEqual(string expectedPath, string actualPath, string diffPath)
    {
        using (var expected = new Bitmap(expectedPath))
        using (var actual = new Bitmap(actualPath))
        {
            int width = Math.Min(expected.Width, actual.Width);
            int height = Math.Min(expected.Height, actual.Height);
            long changed = 0;
            string firstChange = "";
            for (int y = 0; y < height; y++)
            {
                for (int x = 0; x < width; x++)
                {
                    int expectedRgb = Rgb(expected.GetPixel(x, y));
                    int actualRgb = Rgb(actual.GetPixel(x, y));
                    if (expectedRgb != actualRgb && ++changed == 1)
                    {
                        firstChange = string.Format(
                            " First RGB difference at ({0}, {1}): expected #{2:X6}, actual #{3:X6}.",
                            x, y, expectedRgb, actualRgb);
                    }
                }
            }

            if (changed == 0 && expected.Size == actual.Size)
                return;

            string message = string.Format(
                "Display PNG mismatch: expected {0}x{1} ('{2}'), actual {3}x{4} ('{5}'). " +
                "{6} differing RGB pixel(s) in the overlap.{7}",
                expected.Width, expected.Height, expectedPath,
                actual.Width, actual.Height, actualPath, changed, firstChange);
            try
            {
                WriteDiff(expected, actual, diffPath);
            }
            catch (Exception error)
            {
                throw new InvalidOperationException(
                    message + " Could not create diff PNG at '" + diffPath + "': " + error.Message, error);
            }

            throw new InvalidOperationException(message + " Diff PNG: '" + diffPath +
                "' (magenta: RGB mismatch; orange: expected only; cyan: actual only).");
        }
    }

    private static int Rgb(Color color)
    {
        return color.ToArgb() & 0x00FFFFFF;
    }

    private static void WriteDiff(Bitmap expected, Bitmap actual, string path)
    {
        using (var diff = new Bitmap(Math.Max(expected.Width, actual.Width),
                                     Math.Max(expected.Height, actual.Height)))
        {
            for (int y = 0; y < diff.Height; y++)
            {
                for (int x = 0; x < diff.Width; x++)
                {
                    bool inExpected = x < expected.Width && y < expected.Height;
                    bool inActual = x < actual.Width && y < actual.Height;
                    Color marker;
                    if (inExpected && inActual)
                    {
                        Color pixel = expected.GetPixel(x, y);
                        int shade = 16 + (pixel.R + pixel.G + pixel.B) / 12;
                        marker = Rgb(pixel) == Rgb(actual.GetPixel(x, y))
                            ? Color.FromArgb(shade, shade, shade)
                            : Color.Magenta;
                    }
                    else
                    {
                        marker = inExpected ? Color.Orange : inActual ? Color.Cyan : Color.Black;
                    }
                    diff.SetPixel(x, y, marker);
                }
            }

            SaveNewPng(diff, path);
        }
    }

    private static void SaveNewPng(Bitmap image, string path)
    {
        // A new file cannot alias and overwrite either input.
        using (var output = new FileStream(path, FileMode.CreateNew, FileAccess.Write))
            image.Save(output, ImageFormat.Png);
    }

    public static void SelfTest(string directory)
    {
        string expected = Path.Combine(directory, "expected.png");
        string equal = Path.Combine(directory, "equal.png");
        string changed = Path.Combine(directory, "changed.png");
        string reshaped = Path.Combine(directory, "reshaped.png");
        string equalDiff = Path.Combine(directory, "equal-diff.png");
        string changedDiff = Path.Combine(directory, "changed-diff.png");
        string reshapedDiff = Path.Combine(directory, "reshaped-diff.png");
        Color first = Color.FromArgb(80, 160, 240);
        Color second = Color.FromArgb(20, 40, 60);
        WriteFixture(expected, 2, 1, first, second);
        WriteFixture(equal, 2, 1, Color.FromArgb(128, first), second);
        WriteFixture(changed, 2, 1, first, Color.FromArgb(20, 40, 61));
        WriteFixture(reshaped, 1, 2, first, second);

        AssertEqual(expected, equal, equalDiff);
        if (File.Exists(equalDiff))
            throw new InvalidOperationException("Self-test created a diff for equal RGB pixels.");

        ExpectMismatch(expected, changed, changedDiff);
        using (var diff = new Bitmap(changedDiff))
        {
            if (diff.Width != 2 || diff.Height != 1 ||
                Rgb(diff.GetPixel(0, 0)) == Rgb(Color.Magenta) ||
                Rgb(diff.GetPixel(1, 0)) != Rgb(Color.Magenta))
                throw new InvalidOperationException("Self-test diff did not mark the changed pixel.");
        }

        ExpectMismatch(expected, reshaped, reshapedDiff);
        using (var diff = new Bitmap(reshapedDiff))
        {
            if (diff.Width != 2 || diff.Height != 2 ||
                Rgb(diff.GetPixel(1, 0)) != Rgb(Color.Orange) ||
                Rgb(diff.GetPixel(0, 1)) != Rgb(Color.Cyan))
                throw new InvalidOperationException("Self-test diff did not mark the nonoverlapping pixels.");
        }

        string rows = Path.Combine(directory, "rows.png");
        string shifted = Path.Combine(directory, "shifted-rows.png");
        string missing = Path.Combine(directory, "missing-row.png");
        using (var normal = new Bitmap(2, 15))
        using (var moved = new Bitmap(2, 15))
        {
            for (int row = 0; row < 5; row++)
            {
                Color text = row == 0 ? Color.Lime : Color.Red;
                normal.SetPixel(0, 3 * row, text);
                moved.SetPixel(0, 3 * row + 1, text);
            }
            SaveNewPng(normal, rows);
            SaveNewPng(moved, shifted);
            moved.SetPixel(0, 13, Color.Black);
            SaveNewPng(moved, missing);
        }
        AssertSameRows(rows, rows);
        ExpectRowMismatch(rows, shifted);
        ExpectRowMismatch(rows, missing);
    }

    private static void ExpectRowMismatch(string expected, string actual)
    {
        try
        {
            AssertSameRows(expected, actual);
        }
        catch (InvalidOperationException)
        {
            return;
        }
        throw new InvalidOperationException("Self-test failed to detect changed display rows in " + actual + ".");
    }

    private static void WriteFixture(string path, int width, int height, params Color[] pixels)
    {
        using (var image = new Bitmap(width, height))
        {
            for (int i = 0; i < pixels.Length; i++)
                image.SetPixel(i % width, i / width, pixels[i]);
            SaveNewPng(image, path);
        }
    }

    private static void ExpectMismatch(string expected, string actual, string diff)
    {
        try
        {
            AssertEqual(expected, actual, diff);
        }
        catch (InvalidOperationException error)
        {
            if (error.InnerException != null)
                throw;
            return;
        }
        throw new InvalidOperationException("Self-test failed to detect a mismatch for " + actual + ".");
    }
}
