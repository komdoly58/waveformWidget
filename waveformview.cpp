#include "waveformview.h"
#include <iostream>

// #include "mediaProcessor/peaks.h"

#include "renderer.h"

QColor WavBackColor = QColor(11, 19, 43);
QColor WavColor = QColor(111, 255, 233);
QColor RangeColor1 = QColor(62, 120, 178);
QColor RangeColor2 = QColor(241, 136, 5);
QColor RangeColorNonEditable = QColor(141, 153, 174);
QColor SelectionColor = QColor(255, 255, 255, 50);
QColor MinBlankColor = QColor(255, 255, 255, 80);
QColor CursorColor = QColor(74, 49, 77);

WaveformViewport::WaveformViewport(AbstractRenderer *rend, Peaks &&pdata, SubtitleData &&sdata, QWidget *parent) :
        QOpenGLWidget(parent),
        PData(std::move(pdata)),
        SData(std::move(sdata)),
        Rend(rend),
        FocusedSubtitle(SData.end())
{
    if(SData.hasVO())
    {
        DisplayRangeLists.push_back(SData.vo());
    }
    DisplayRangeLists.push_back(SData.subs());
    SData.setSelectedSubtitle(SData.begin());
    Selection = SData.selectedSubtitle()->Time;

    FocusedSubtitle = SData.end(); // No Focused Subtitle

    //connect(&PlayCursorUpdater, SIGNAL(timeout()), this, SLOT(updatePlayCursorPos()));
    //PlayCursorUpdater.start(UpdateIntervalMs);

    connect(Rend, SIGNAL(positionChanged(int)), this, SLOT(updatePlayCursorPos(int)));

    // Receive move events even when no mouse button is clicked
    setMouseTracking(true);
}

void WaveformViewport::paintWav(QPainter &painter)
{
    double PeaksPerSecond = double(PData.sampleRate()) / PData.samplesPerPeak();
    double SecondsPerPixel = (PageSizeMs / 1000.0) / width();
    double PeaksPerPixel = PeaksPerSecond * SecondsPerPixel;

    // First Peak visible
    int StartPeak = std::round((PeaksPerSecond / 1000.0) * PositionMs);

    int pixel_start = 0;
    int pixel_end = width();

    unsigned int peaks_per_pixel = std::round(PeaksPerPixel);
    unsigned int peakIndex;

    // Peak to be shown
    int peakMin, peakMax;
    // Scaled Peak
    int scaledPeakMin, scaledPeakMax;


    QRect WavRect = painter.window();
    // Leave room for the ruler, if it is to be shown
    WavRect.setBottom(WavRect.bottom() - RulerHeight);

    painter.fillRect(WavRect, WavBackColor);

    int Middle = (WavRect.height() - 1) / 2;

    painter.setPen(WavColor);

    /* TODO: Use directly data extracted from file, if avaible, if zoom is huge:
     * if(PeaksPerPixel < 1.0)
     * {
     * ....
     * }
     */

    for(int curr_pixel = pixel_start; curr_pixel < pixel_end; ++curr_pixel)
    {
        peakIndex = std::round(PeaksPerPixel * curr_pixel) + StartPeak;

        if(peakIndex >= PData.peaksNumber()) peakIndex = PData.peaksNumber() - 1;

        peakMin = PData[peakIndex].min();
        peakMax = PData[peakIndex].max();

        // If more than one peak per pixel needs to be shown, calculate the maximum and the minimum peaks among them and represent that peak
        for(unsigned int peakCount = 1; peakIndex + peakCount < PData.peaksNumber() && peakCount < peaks_per_pixel; ++peakCount)
        {
            if(PData[peakIndex + peakCount].min() < peakMin) peakMin = PData[peakIndex + peakCount].min();
            if(PData[peakIndex + peakCount].max() > peakMax) peakMax = PData[peakIndex + peakCount].max();
        }

        // Get scaled peaks value
        scaledPeakMax = std::round((((peakMax * VerticalScaling) / 100.0) * WavRect.height()) / 65536);
        scaledPeakMin = std::round((((peakMin * VerticalScaling) / 100.0) * WavRect.height()) / 65536);

        painter.drawLine(QPoint(curr_pixel, Middle - scaledPeakMax), QPoint(curr_pixel, Middle - scaledPeakMin));

    }
    painter.drawLine(QPoint(pixel_start, Middle), QPoint(pixel_end - 1, Middle));
}

void WaveformViewport::paintRuler(QPainter &painter)
{
    Q_UNUSED(painter)
}

void WaveformViewport::paintRanges(QPainter &painter, RangeList &Subs, int topPos, int bottomPos, bool topLine, bool bottomLine)
{
    QColor Colors[2];
    if(Subs.editable())
    {
        Colors[0] = RangeColor1;
        Colors[1] = RangeColor2;
    }
    else
    {
        Colors[0] = Colors[1] = RangeColorNonEditable;
    }

    int h_line_begin;
    int h_line_end;

    int RangeHeightDiv10 = (bottomPos - topPos) / 10;
    int y1 = topPos + RangeHeightDiv10;
    int y2 = bottomPos - RangeHeightDiv10;

    RangeList::iterator subs = Subs.subsAheadOf(PositionMs);
    while(subs != Subs.end() && subs->Time.StartTime <= PositionMs + PageSizeMs)
    {
        painter.setPen(Colors[subs->Number % 2]);
        h_line_begin = 0;
        h_line_end = width() - 1;
        if(isPositionVisible(subs->Time.StartTime))
        {
            QPen OldPen = painter.pen();
            QPen NewPen = OldPen;
            // If time is focused draw solid line, otherwise dot line
            if(FocusMode == FocusBegin && FocusedSubtitle == subs)
            {
                NewPen.setStyle(Qt::SolidLine);
            }
            else
            {
                NewPen.setStyle(Qt::DotLine);
            }
            painter.setPen(NewPen);

            h_line_begin = relTimeToPixel(subs->Time.StartTime);
            painter.drawLine(QPoint(h_line_begin, topPos), QPoint(h_line_begin, bottomPos));

            painter.setPen(OldPen);
        }
        if(isPositionVisible(subs->Time.EndTime))
        {
            QPen OldPen = painter.pen();
            QPen NewPen = OldPen;
            // If time is focused draw solid line, otherwise dot line
            if(FocusMode == FocusEnd && FocusedSubtitle == subs)
            {
                NewPen.setStyle(Qt::SolidLine);
            }
            else
            {
                NewPen.setStyle(Qt::DotLine);
            }
            painter.setPen(NewPen);

            h_line_end = relTimeToPixel(subs->Time.EndTime);
            painter.drawLine(QPoint(h_line_end, topPos), QPoint(h_line_end, bottomPos));

            painter.setPen(OldPen);
        }
        if(topLine)
        {
            painter.drawLine(QPoint(h_line_begin, y1), QPoint(h_line_end, y1));
        }
        if(bottomLine)
        {
            painter.drawLine(QPoint(h_line_begin, y2), QPoint(h_line_end, y2));
        }

        if(h_line_end - h_line_begin > 10)
        {
            const int TextMargins = 5, MinSpace = 25;
            QRect CustomDrawRect(QPoint(h_line_begin + TextMargins, y1), QPoint(h_line_end + TextMargins, y2));
            if(CustomDrawRect.width() > MinSpace)
            {
                QTextOption Opt = topLine ? QTextOption() : QTextOption(Qt::AlignBottom);
                painter.drawText(CustomDrawRect, subs->Text, Opt);
            }
        }
        ++subs;
    }
}

void WaveformViewport::paintRangeLists(QPainter &painter)
{
    int Height = height() - RulerHeight;
    int SubHeight = Height / DisplayRangeLists.size();
    unsigned int i = 0;
    for(auto subsList = DisplayRangeLists.cbegin(); subsList != DisplayRangeLists.cend(); ++subsList, ++i)
    {
        paintRanges(painter, **subsList, SubHeight * i, SubHeight * (i+1), i == 0, i == DisplayRangeLists.size() - 1);
    }
}

void WaveformViewport::paintSelection(QPainter &painter)
{
    if(Selection.StartTime >= 0)
    {
        if(Selection.duration() == 0 && isPositionVisible(Selection.StartTime))
        {
            int x = relTimeToPixel(Selection.StartTime);
            painter.drawLine(QPoint(x, 0), QPoint(x, height() - RulerHeight));
        }
        else
        {
            int x1 = Selection.StartTime >= PositionMs ? relTimeToPixel(Selection.StartTime) : 0;
            int x2 = Selection.EndTime <= PositionMs + PageSizeMs ? relTimeToPixel(Selection.EndTime) : width() - 1;

            QRect SelRect = painter.window();
            SelRect.setLeft(x1);
            SelRect.setRight(x2);
            SelRect.setBottom(SelRect.bottom() - RulerHeight);

            // InvertRect
            painter.fillRect(SelRect, SelectionColor);
        }

    }
}

void WaveformViewport::paintCursor(QPainter &painter)
{
    painter.setPen(QPen(CursorColor, 1, Qt::DotLine));
    int x_pos = relTimeToPixel(CursorMs);
    painter.drawLine(QPoint(x_pos, 0), QPoint(x_pos, height() - RulerHeight));
}

void WaveformViewport::paintPlayCursor(QPainter &painter)
{
    if(IsPlaying && isPositionVisible(PlayCursorMs))
    {
        painter.setPen(SelectionColor);
        int x = relTimeToPixel(PlayCursorMs);
        painter.drawLine(QPoint(x, 0), QPoint(x, height() - RulerHeight));
    }
}

void WaveformViewport::paintMinimumBlank(QPainter &painter, int rangeTop, int rangeBottom)
{
    if(ShowMinBlank)
    {
        if(Info1.exists())
        {
            QRect MinBlankRect(QPoint(Info1.getStart(MinimumBlankMs), rangeTop), QPoint(Info1.getEnd(MinimumBlankMs), rangeBottom));
            painter.fillRect(MinBlankRect, MinBlankColor);
        }
        if(Info2.exists())
        {
            QRect MinBlankRect(QPoint(Info2.getStart(MinimumBlankMs), rangeTop), QPoint(Info2.getEnd(MinimumBlankMs), rangeBottom));
            painter.fillRect(MinBlankRect, MinBlankColor);
        }
    }
}
