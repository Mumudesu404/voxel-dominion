# Build original vector numeral artwork; the supplied UI images are never modified.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$DigitBuilderSource = @'
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Globalization;
using System.IO;
using System.Text;

public static class VoxelGoldDigits {
    static float[][][] Glyphs = new float[][][] {
        new float[][] {new float[]{10,0,54,0,64,10,64,80,54,90,10,90,0,80,0,10},new float[]{22,22,42,22,42,68,22,68}},
        new float[][] {new float[]{6,14,23,0,43,0,43,68,58,68,58,90,4,90,4,68,20,68,20,28,6,28}},
        new float[][] {new float[]{0,10,10,0,54,0,64,10,64,40,22,65,22,68,64,68,64,90,0,90,0,54,42,30,42,22,22,22,22,31,0,31}},
        new float[][] {new float[]{0,0,54,0,64,10,64,35,54,45,64,55,64,80,54,90,0,90,0,68,42,68,42,56,16,56,16,34,42,34,42,22,0,22}},
        new float[][] {new float[]{0,0,22,0,22,34,39,34,39,0,61,0,61,90,39,90,39,56,0,56}},
        new float[][] {new float[]{0,0,64,0,64,22,22,22,22,35,54,35,64,45,64,80,54,90,10,90,0,80,0,65,22,65,22,68,42,68,42,57,0,57}},
        new float[][] {new float[]{10,0,64,0,64,22,22,22,22,34,54,34,64,44,64,80,54,90,10,90,0,80,0,10},new float[]{22,56,42,56,42,68,22,68}},
        new float[][] {new float[]{0,0,64,0,64,23,39,90,15,90,40,22,0,22}},
        new float[][] {new float[]{10,0,54,0,64,10,64,80,54,90,10,90,0,80,0,10},new float[]{22,20,42,20,42,35,22,35},new float[]{22,55,42,55,42,70,22,70}},
        new float[][] {new float[]{10,0,54,0,64,10,64,80,54,90,0,90,0,68,42,68,42,56,10,56,0,46,0,10},new float[]{22,22,42,22,42,34,22,34}}
    };
    static GraphicsPath PathFor(int digit) {
        var path=new GraphicsPath(FillMode.Alternate);
        foreach(var polygon in Glyphs[digit]) {
            var points=new PointF[polygon.Length/2];
            for(int i=0;i<points.Length;i++)points[i]=new PointF(polygon[i*2],polygon[i*2+1]);
            path.AddPolygon(points);
        }
        return path;
    }
    static string SvgPath(int digit) {
        var b=new StringBuilder();
        foreach(var polygon in Glyphs[digit]) {
            for(int i=0;i<polygon.Length;i+=2)b.Append(i==0?"M":"L").Append(polygon[i].ToString(CultureInfo.InvariantCulture)).Append(",").Append(polygon[i+1].ToString(CultureInfo.InvariantCulture));
            b.Append("Z");
        }
        return b.ToString();
    }
    public static void Build(string folder) {
        Directory.CreateDirectory(folder);
        var svg=new StringBuilder("<svg xmlns='http://www.w3.org/2000/svg' width='960' height='128' viewBox='0 0 960 128'><defs><linearGradient id='gold' x1='0' y1='0' x2='0' y2='1'><stop offset='0' stop-color='#fff4b0'/><stop offset='1' stop-color='#ffbd35'/></linearGradient></defs>");
        using(var bmp=new Bitmap(3840,512,PixelFormat.Format32bppArgb))using(var g=Graphics.FromImage(bmp)) {
            g.Clear(Color.Transparent);g.SmoothingMode=SmoothingMode.AntiAlias;g.PixelOffsetMode=PixelOffsetMode.HighQuality;
            g.ScaleTransform(4,4);
            for(int digit=0;digit<10;digit++) {
                var saved=g.Save();g.TranslateTransform(digit*96+16,13);
                using(var path=PathFor(digit))using(var extrusion=(GraphicsPath)path.Clone())using(var shift=new Matrix()) {
                    shift.Translate(2,6);extrusion.Transform(shift);
                    using(var pen=new Pen(Color.FromArgb(255,28,12,6),10)){pen.LineJoin=LineJoin.Miter;g.DrawPath(pen,extrusion);}
                    using(var brush=new SolidBrush(Color.FromArgb(255,100,45,12)))g.FillPath(brush,extrusion);
                    using(var pen=new Pen(Color.FromArgb(255,70,30,12),7)){pen.LineJoin=LineJoin.Miter;g.DrawPath(pen,path);}
                    using(var face=new LinearGradientBrush(new RectangleF(0,0,64,90),Color.FromArgb(255,255,244,176),Color.FromArgb(255,255,189,53),90))g.FillPath(face,path);
                    using(var edge=new Pen(Color.FromArgb(255,255,232,138),1.5f)){edge.LineJoin=LineJoin.Miter;g.DrawPath(edge,path);}
                }
                g.Restore(saved);
                string d=SvgPath(digit);svg.Append("<g transform='translate(").Append(digit*96+16).Append(",13)' fill-rule='evenodd' stroke-linejoin='miter'>");
                svg.Append("<path d='").Append(d).Append("' transform='translate(2,6)' fill='#642d0c' stroke='#1c0c06' stroke-width='10'/>");
                svg.Append("<path d='").Append(d).Append("' fill='url(#gold)' stroke='#461e0c' stroke-width='7'/><path d='").Append(d).Append("' fill='none' stroke='#ffe88a' stroke-width='1.5'/></g>");
            }
            bmp.Save(System.IO.Path.Combine(folder,"GoldDigits.png"),ImageFormat.Png);
        }
        svg.Append("</svg>");File.WriteAllText(System.IO.Path.Combine(folder,"GoldDigits.svg"),svg.ToString(),new UTF8Encoding(false));
    }
}
'@
Add-Type -TypeDefinition $DigitBuilderSource -ReferencedAssemblies System.Drawing
[VoxelGoldDigits]::Build((Join-Path (Split-Path $PSScriptRoot -Parent) 'SourceArt/UI'))
Write-Output 'VOXEL_GOLD_DIGITS_OK: original vector outlines, 3840x512 transparent atlas'
