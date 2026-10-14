#pragma once
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <stdexcept>

namespace ms {
// GDI+ is part of Windows. It gives the native controls smooth vector decoration.
class UiSession {
public:
    UiSession() {
        Gdiplus::GdiplusStartupInput input;
        if(Gdiplus::GdiplusStartup(&token_,&input,nullptr)!=Gdiplus::Ok)
            throw std::runtime_error("Cannot initialize GDI+");
    }
    ~UiSession() { Gdiplus::GdiplusShutdown(token_); }
private:
    ULONG_PTR token_=0;
};
class UiCanvas {
public:
    UiCanvas(HDC dc,UINT dpi) : graphics_(dc) {
        graphics_.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
        graphics_.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        const float factor=static_cast<float>(dpi)/96.0F;
        graphics_.ScaleTransform(factor,factor);
    }
    void rounded(float x,float y,float width,float height,float radius,Gdiplus::Color fill,
                 Gdiplus::Color border=Gdiplus::Color(0,0,0,0)) {
        Gdiplus::GraphicsPath path;
        const float d=radius*2;
        path.AddArc(x,y,d,d,180,90);
        path.AddArc(x+width-d,y,d,d,270,90);
        path.AddArc(x+width-d,y+height-d,d,d,0,90);
        path.AddArc(x,y+height-d,d,d,90,90);
        path.CloseFigure();
        Gdiplus::SolidBrush brush(fill);
        graphics_.FillPath(&brush,&path);
        if(border.GetAlpha()) { Gdiplus::Pen pen(border,0.7F); graphics_.DrawPath(&pen,&path); }
    }
    void line(float x1,float y1,float x2,float y2,Gdiplus::Color color,float width=1.3F) {
        Gdiplus::Pen pen(color,width);
        pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
        graphics_.DrawLine(&pen,x1,y1,x2,y2);
    }
    void circle(float x,float y,float size,Gdiplus::Color color) {
        Gdiplus::SolidBrush brush(color); graphics_.FillEllipse(&brush,x,y,size,size);
    }
    void mouse(float x,float y) {
        rounded(x,y,32,32,9,Gdiplus::Color(255,0,95,184));
        rounded(x+10,y+5,13,22,6,Gdiplus::Color(0,0,0,0),Gdiplus::Color(255,255,255,255));
        line(x+16.5F,y+6,x+16.5F,y+14,Gdiplus::Color(255,255,255,255));
        line(x+7,y+12,x+7,y+15,Gdiplus::Color(255,255,255,255));
        line(x+7,y+19,x+7,y+22,Gdiplus::Color(255,255,255,255));
    }
    void pencil(float x,float y,Gdiplus::Color color) {
        line(x+3,y+10,x+10,y+3,color);
        line(x+5,y+12,x+12,y+5,color);
        line(x+3,y+10,x+2,y+13,color);
        line(x+2,y+13,x+5,y+12,color);
        line(x+10,y+3,x+12,y+5,color);
        line(x+1,y+16,x+15,y+16,color,1.0F);
    }
    void power(float x,float y,Gdiplus::Color color) {
        Gdiplus::Pen pen(color,1.5F); pen.SetStartCap(Gdiplus::LineCapRound); pen.SetEndCap(Gdiplus::LineCapRound);
        graphics_.DrawArc(&pen,x+1,y+2,15.0F,15.0F,310.0F,280.0F);
        line(x+8.5F,y,x+8.5F,y+9,color,1.5F);
    }
private:
    Gdiplus::Graphics graphics_;
};
}
