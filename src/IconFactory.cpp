// IconFactory.cpp

#include "IconFactory.h"

#include <vector>

namespace ms {
namespace IconFactory {
namespace {

constexpr int kSupersample = 4;

constexpr int kBgTopR = 0x4A, kBgTopG = 0x7D, kBgTopB = 0xFF;
constexpr int kBgBotR = 0x24, kBgBotG = 0x50, kBgBotB = 0xE8;

constexpr int kPausedTopR = 0xB4, kPausedTopG = 0xBA, kPausedTopB = 0xC4;
constexpr int kPausedBotR = 0x8B, kPausedBotG = 0x92, kPausedBotB = 0x9D;

struct Rgba {
    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    unsigned char a = 0;
};

inline double Clamp(double value, double low, double high) {
    return value < low ? low : (value > high ? high : value);
}

// 点是否落在归一化坐标下的圆角矩形内
bool InRoundedRect(double x, double y,
                   double x0, double y0, double x1, double y1,
                   double radius) {
    if (x < x0 || x > x1 || y < y0 || y > y1) {
        return false;
    }
    const double cx = Clamp(x, x0 + radius, x1 - radius);
    const double cy = Clamp(y, y0 + radius, y1 - radius);
    const double dx = x - cx;
    const double dy = y - cy;
    return dx * dx + dy * dy <= radius * radius;
}

unsigned char LerpChannel(int from, int to, double t) {
    const double value = static_cast<double>(from) + (static_cast<double>(to) - from) * t;
    return static_cast<unsigned char>(Clamp(value + 0.5, 0.0, 255.0));
}

Rgba Gradient(int topR, int topG, int topB, int botR, int botG, int botB, double t) {
    Rgba out;
    out.r = LerpChannel(topR, botR, t);
    out.g = LerpChannel(topG, botG, t);
    out.b = LerpChannel(topB, botB, t);
    out.a = 255;
    return out;
}

// 运行态：蓝色圆角方块 + 白色鼠标剪影
Rgba SampleRunning(double x, double y) {
    if (!InRoundedRect(x, y, 0.03, 0.03, 0.97, 0.97, 0.24)) {
        return Rgba{};
    }

    if (InRoundedRect(x, y, 0.310, 0.155, 0.690, 0.845, 0.190)) {
        const bool splitH = (y > 0.398 && y < 0.442);                    // 左右键分界线
        const bool splitV = (x > 0.482 && x < 0.518) && (y < 0.420);     // 双键中缝
        const bool wheel  = InRoundedRect(x, y, 0.452, 0.240, 0.548, 0.352, 0.048);
        if (!splitH && !splitV && !wheel) {
            return Rgba{255, 255, 255, 255};
        }
    }

    return Gradient(kBgTopR, kBgTopG, kBgTopB, kBgBotR, kBgBotG, kBgBotB, y);
}

// 暂停态：灰色圆角方块 + 白色暂停条
Rgba SamplePaused(double x, double y) {
    if (!InRoundedRect(x, y, 0.03, 0.03, 0.97, 0.97, 0.24)) {
        return Rgba{};
    }

    const bool barLeft  = InRoundedRect(x, y, 0.340, 0.270, 0.446, 0.730, 0.052);
    const bool barRight = InRoundedRect(x, y, 0.554, 0.270, 0.660, 0.730, 0.052);
    if (barLeft || barRight) {
        return Rgba{255, 255, 255, 255};
    }

    return Gradient(kPausedTopR, kPausedTopG, kPausedTopB,
                    kPausedBotR, kPausedBotG, kPausedBotB, y);
}

// 绘制一张 32bpp 预乘无关的 DIB 到 bits（BGRA 顺序，alpha 直通）
void Rasterize(unsigned int* bits, int size, bool paused) {
    const double step  = 1.0 / (static_cast<double>(size) * kSupersample);
    const double half  = step * 0.5;
    const int    total = kSupersample * kSupersample;

    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            unsigned int sumR = 0;
            unsigned int sumG = 0;
            unsigned int sumB = 0;
            int covered = 0;

            for (int sy = 0; sy < kSupersample; ++sy) {
                const double y = (py * kSupersample + sy) * step + half;
                for (int sx = 0; sx < kSupersample; ++sx) {
                    const double x = (px * kSupersample + sx) * step + half;
                    const Rgba sample = paused ? SamplePaused(x, y) : SampleRunning(x, y);
                    if (sample.a != 0) {
                        sumR += sample.r;
                        sumG += sample.g;
                        sumB += sample.b;
                        ++covered;
                    }
                }
            }

            unsigned int pixel = 0;
            if (covered != 0) {
                const unsigned int r = sumR / static_cast<unsigned int>(covered);
                const unsigned int g = sumG / static_cast<unsigned int>(covered);
                const unsigned int b = sumB / static_cast<unsigned int>(covered);
                const unsigned int a =
                    (static_cast<unsigned int>(covered) * 255u) / static_cast<unsigned int>(total);
                pixel = (a << 24) | (r << 16) | (g << 8) | b;
            }
            bits[py * size + px] = pixel;
        }
    }
}

HICON BuildIcon(int size, bool paused) {
    if (size < 8) {
        size = 8;
    }

    // 32bpp 自上而下 DIB：第 4 个字节即 alpha 通道
    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth       = size;
    bitmapInfo.bmiHeader.biHeight      = -size;
    bitmapInfo.bmiHeader.biPlanes      = 1;
    bitmapInfo.bmiHeader.biBitCount    = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    HDC screenDc = GetDC(nullptr);
    if (screenDc == nullptr) {
        return nullptr;
    }

    void*   bits  = nullptr;
    HBITMAP color = CreateDIBSection(screenDc, &bitmapInfo, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, screenDc);

    if (color == nullptr || bits == nullptr) {
        if (color != nullptr) {
            DeleteObject(color);
        }
        return nullptr;
    }

    Rasterize(static_cast<unsigned int*>(bits), size, paused);

    // 颜色位图已携带 alpha，掩码位图保持全 0 即可（显式清零，避免内容不确定）
    const int maskStride = ((size + 15) / 16) * 2;
    std::vector<unsigned char> maskBits(static_cast<size_t>(maskStride) * size, 0);
    HBITMAP mask = CreateBitmap(size, size, 1, 1, maskBits.data());

    ICONINFO info{};
    info.fIcon    = TRUE;
    info.hbmColor = color;
    info.hbmMask  = mask;

    HICON icon = CreateIconIndirect(&info);

    DeleteObject(color);
    if (mask != nullptr) {
        DeleteObject(mask);
    }
    return icon;
}

}  // namespace

HICON CreateTrayIcon(bool paused) {
    int size = GetSystemMetrics(SM_CXSMICON);
    if (size < 16) {
        size = 16;
    }
    HICON icon = BuildIcon(size, paused);
    if (icon == nullptr) {
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    return icon;
}

HICON CreateAppIcon(bool paused) {
    HICON icon = BuildIcon(32, paused);
    if (icon == nullptr) {
        icon = LoadIconW(nullptr, IDI_APPLICATION);
    }
    return icon;
}

}  // namespace IconFactory
}  // namespace ms
