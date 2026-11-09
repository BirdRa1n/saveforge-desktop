// Renders the app icon SVG to square PNGs with Qt's SVG renderer.
//
//   render <icon.svg> <out-dir> <size>...
//
// Writes <out-dir>/icon-<size>.png for each size. Built and run by
// tools/icons/generate.py; not part of the app build.

#include <QCoreApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <cstdio>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList args = QCoreApplication::arguments();
    if (args.size() < 4) {
        std::fprintf(stderr, "usage: render <icon.svg> <out-dir> <size>...\n");
        return 2;
    }
    QSvgRenderer renderer(args.at(1));
    if (!renderer.isValid()) {
        std::fprintf(stderr, "cannot read %s\n", qPrintable(args.at(1)));
        return 1;
    }
    for (const QString &arg : args.mid(3)) {
        const int size = arg.toInt();
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);
        renderer.render(&painter);
        painter.end();
        const QString out = args.at(2) + QStringLiteral("/icon-%1.png").arg(size);
        if (!image.save(out)) {
            std::fprintf(stderr, "cannot write %s\n", qPrintable(out));
            return 1;
        }
    }
    return 0;
}
