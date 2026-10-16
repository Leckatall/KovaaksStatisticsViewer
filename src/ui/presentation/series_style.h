#ifndef KOVAAKSSTATISTICSVIEWER_SERIES_STYLE_H
#define KOVAAKSSTATISTICSVIEWER_SERIES_STYLE_H

#include <QColor>
#include <string_view>

#include "app/contracts/graph_presentation.h"
#include "series_model.h"

namespace ksv::presentation {
    // An absent committed colour leaves the series neutral: GraphCanvas then draws the foreground colour.
    inline void applySeriesStyle(SeriesModel &series, const application::GraphPresentation &presentation,
                                 const std::string_view seriesKey) {
        const auto style = presentation.seriesStyle(seriesKey);
        series.setColor(style.color ? QColor(style.color->red, style.color->green, style.color->blue,
                                              style.color->alpha)
                                    : QColor());
        series.lineWidth = style.width;
    }
}

#endif //KOVAAKSSTATISTICSVIEWER_SERIES_STYLE_H
