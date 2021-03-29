#include <QApplication>
#include "annotationvisualizer.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName(AnnotationVisualizer::tr("Annotation visualizer"));
    AnnotationVisualizer annotationVisualizer;
    annotationVisualizer.show();
    return app.exec();
}
